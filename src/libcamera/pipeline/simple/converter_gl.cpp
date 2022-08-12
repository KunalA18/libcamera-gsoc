/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Copyright (C) 2022, Kunal Agarwal
 *
 * converter_gl.cpp - GL converter for debayering
 */

#include <gbm.h>
#include <limits.h>

#include <libcamera/base/unique_fd.h>

#include <libcamera/formats.h>
#include <libcamera/framebuffer.h>

#include "libcamera/internal/formats.h"

#include <GL/glew.h>

#include "texture.h"
#include "converter_gl.h"

namespace libcamera {

LOG_DECLARE_CATEGORY(SimplePipeline)

float rectangleVertices[] = {
	/* Coords */ /* texCoords */
	1.0f, -1.0f, 1.0f, 0.0f,
	-1.0f, -1.0f, 0.0f, 0.0f,
	-1.0f, 1.0f, 0.0f, 1.0f,

	1.0f, 1.0f, 1.0f, 1.0f,
	1.0f, -1.0f, 1.0f, 0.0f,
	-1.0f, 1.0f, 0.0f, 1.0f
};

int SimpleConverter::configure(const StreamConfiguration &inputCfg,
			       const std::vector<std::reference_wrapper<StreamConfiguration>> &outputCfgs)
{
	int ret = configureGL(inputCfg, outputCfgs.front());
	return ret;
}

int SimpleConverter::configureGL(const StreamConfiguration &inputCfg,
				 const StreamConfiguration &outputCfg)
{
	informat_.size = inputCfg.size;
	informat_.planes[0].bpl_ = inputCfg.stride;
	outformat_.size = outputCfg.size;
	outformat_.planes[0].bpl_ = outputCfg.stride;
	return 0;
}

std::vector<PixelFormat> SimpleConverter::formats([[maybe_unused]] PixelFormat input)
{
	return {
		PixelFormat::fromString("RGB888"),
		PixelFormat::fromString("ARGB8888"),
	};
}

SizeRange SimpleConverter::sizes(const Size &input)
{
	SizeRange sizes({ 1, 1 }, input);
	return sizes;
}

std::tuple<unsigned int, unsigned int>
SimpleConverter::strideAndFrameSize(const PixelFormat &pixelFormat, const Size &sz)
{
	PixelFormatInfo format;
	format.info(pixelFormat);
	return std::make_tuple(format.stride(sz.width, 0, 1), format.frameSize(sz, 1));
}

int SimpleConverter::exportBuffers(unsigned int output, unsigned int count,
				   std::vector<std::unique_ptr<FrameBuffer>> *buffers)
{
	if (output != 0)
		return -EINVAL;

	if (outputBuffers.size() > 0)
		return -EINVAL;

	std::vector<std::unique_ptr<FrameBuffer>> out;
	for (unsigned i = 0; i < count; ++i) {
		auto tex = createBuffer();
		outputBuffers.emplace_back(tex.second);
		buffers->push_back(std::move(tex.first));
	}
	return count;
}

std::pair<std::unique_ptr<FrameBuffer>, GlRenderTarget> SimpleConverter::createBuffer()
{
	bo = gbm_bo_create(gbm, outformat_.size.width, outformat_.size.height,
			   GBM_BO_FORMAT_ARGB8888, GBM_BO_USE_RENDERING);
	if (!bo)
		LOG(SimplePipeline, Error) << "GBM buffer not created ";

	unsigned int filedesc = gbm_bo_get_fd(bo);

	LOG(SimplePipeline, Debug) << "File Descriptor value: " << filedesc;

	DmabufImage dimg = importDmabuf(filedesc, outformat_.size, libcamera::formats::ARGB8888);

	std::vector<FrameBuffer::Plane> planes;
	UniqueFD fd(filedesc);
	FrameBuffer::Plane plane;
	plane.fd = SharedFD(std::move(fd));
	plane.offset = gbm_bo_get_offset(bo, 0);
	plane.length = gbm_bo_get_stride_for_plane(bo, 0) * outformat_.size.height;

	planes.push_back(std::move(plane));

	auto fb = std::make_unique<FrameBuffer>(planes);
	return std::make_pair(std::move(fb), GlRenderTarget(fb.get(), dimg));
}

SimpleConverter::DmabufImage SimpleConverter::importDmabuf(int fdesc, Size pixelSize, PixelFormat format)
{
	int bytes_per_pixel = 4;
	EGLint const attrs[] = {
		EGL_WIDTH,
		(int)pixelSize.width,
		EGL_HEIGHT,
		(int)pixelSize.height,
		EGL_LINUX_DRM_FOURCC_EXT,
		(int)format.fourcc(),
		EGL_DMA_BUF_PLANE0_FD_EXT,
		fdesc,
		EGL_DMA_BUF_PLANE0_OFFSET_EXT,
		0,
		EGL_DMA_BUF_PLANE0_PITCH_EXT,
		(int)pixelSize.width * bytes_per_pixel,
		EGL_NONE,
	};

	auto eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
	EGLImageKHR image = eglCreateImageKHR(
		display_,
		EGL_NO_CONTEXT,
		EGL_LINUX_DMA_BUF_EXT,
		NULL,
		attrs);

	int e = glGetError();

	if (e != GL_NO_ERROR)
		LOG(SimplePipeline, Error) << "GL_ERROR: " << e;

	GLuint texture;
	glGenTextures(1, &texture);
	struct DmabufImage img = {
		.texture = texture,
		.image = image,
	};

	glBindTexture(GL_TEXTURE_2D, texture);
	auto glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
	glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);

	return img;
}

int SimpleConverter::start()
{
	eglBindAPI(EGL_OPENGL_API);
	device_ = open("/dev/dri/card0", O_RDWR);

	if (!device_)
		LOG(SimplePipeline, Error) << "GBM Device not opened ";

	gbm = gbm_create_device(device_);

	if (!gbm)
		LOG(SimplePipeline, Error) << " GBM Device not created ";

	auto eglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");

	/* get an EGL display connection */
	display_ = eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_MESA, gbm, NULL);

	/* initialize the EGL display connection */
	eglInitialize(display_, NULL, NULL);
	EGLConfig config;
	EGLint n_of_configs;

	eglGetConfigs(display_, &config, 1, &n_of_configs);

	context_ = eglCreateContext(display_, config, EGL_NO_CONTEXT, NULL);

	if (context_ == EGL_NO_CONTEXT) {
		EGLint err = eglGetError();
		LOG(SimplePipeline, Error) << " Context creation failed: " << err;
		return -1;
	}

	/* connect the context to the surface */
	eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, context_);

	/*Load GLEW so it configures OpenGL */
	glewInit();

	shaderProgram_.callShader("default.vert", "default.frag");
	framebufferProgram_.callShader("bayer_8.vert", "bayer_8.frag");
	framebufferProgram_.activate();
	glBindAttribLocation(framebufferProgram_.id_, 0, "vertexIn");
	glBindAttribLocation(framebufferProgram_.id_, 2, "textureIn");
	glUniform1i(glGetUniformLocation(framebufferProgram_.id_, "tex_y"), 0);
	glUniform2f(glGetUniformLocation(framebufferProgram_.id_, "tex_step"), 1.0f / (informat_.planes[0].bpl_ - 1),
		    1.0f / (informat_.size.height - 1));
	glUniform2i(glGetUniformLocation(framebufferProgram_.id_, "tex_size"), informat_.size.width,
		    informat_.size.height);
	glUniform2f(glGetUniformLocation(framebufferProgram_.id_, "tex_bayer_first_red"), 0.0, 1.0);

	/* Prepare framebuffer rectangle VBO and VAO */

	glGenVertexArrays(1, &rectVAO);
	glGenBuffers(1, &rectVBO);
	glBindVertexArray(rectVAO);
	glBindBuffer(GL_ARRAY_BUFFER, rectVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(rectangleVertices), &rectangleVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));

	/* create FrameBuffer object */

	glGenFramebuffers(1, &fbo_);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

	return 0;
}

int SimpleConverter::queueBuffers(FrameBuffer *input,
				  const std::map<unsigned int, FrameBuffer *> &outputs)
{
	int ret;
	if (outputs.empty())
		return -EINVAL;

	for (auto &ib : outputs) {
		ret = queueBufferGL(input, ib.second);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int SimpleConverter::queueBufferGL(FrameBuffer *input, FrameBuffer *output)
{
	DmabufImage rend_tex = importDmabuf(output->planes()[0].fd.get(), outformat_.size, libcamera::formats::ARGB8888);

	Texture bayer(GL_TEXTURE_2D, rend_tex.texture);
	bayer.initTexture(GL_TEXTURE0);
	bayer.startTexture(mappedBuffers_[input].get(), GL_LUMINANCE, GL_UNSIGNED_BYTE, informat_.size);
	bayer.unbind();

	/* Error checking framebuffer*/
	GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "Framebuffer error: " << fboStatus << std::endl;

	/* Main */
	/* Bind the custom framebuffer */
	glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
	/* Specify the color of the background */
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	/* Clean the back buffer and assign the new color to it */
	glClear(GL_COLOR_BUFFER_BIT);
	/* Bind the default framebuffer */
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	/* Draw the framebuffer rectangle */
	framebufferProgram_.activate();
	glBindVertexArray(rectVAO);
	bayer.bind();
	glDrawArrays(GL_TRIANGLES, 0, 6);

	return 0;
}

void SimpleConverter::stop()
{
	/* Delete all the objects we've created */
	shaderProgram_.deleteProgram();
	glDeleteFramebuffers(1, &fbo_);
	eglDestroyContext(display_, context_);
	eglTerminate(display_);

	gbm_bo_destroy(bo);
	gbm_device_destroy(gbm);
	close(device_);
}

} /* namespace libcamera */
