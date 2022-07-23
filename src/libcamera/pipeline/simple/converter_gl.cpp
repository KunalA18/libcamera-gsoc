#include "converter_gl.h"

#include <Texture.h>
#include <gbm.h>
#include <limits.h>

#include <libcamera/base/unique_fd.h>

#include <libcamera/formats.h>
#include <libcamera/framebuffer.h>

#include <libcamera/internal/formats.h>

//#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glew.h>

namespace libcamera {

LOG_DECLARE_CATEGORY(SimplePipeline)

float rectangleVertices[] = {
	// Coords    // texCoords
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
	int ret = 0;
	for (unsigned int i = 0; i < outputCfgs.size(); ++i) {
		ret = configureGL(inputCfg, outputCfgs[i]);
		if (ret < 0)
			break;
	}

	return 0;
}

int SimpleConverter::configureGL(const StreamConfiguration &inputCfg,
				 const StreamConfiguration &outputCfg)
{
	informat.size = inputCfg.size;
	informat.planes[0].bpl = inputCfg.stride;
	outformat.size = outputCfg.size;
	outformat.planes[0].bpl = outputCfg.stride;
	return 0;
}

std::vector<PixelFormat> SimpleConverter::formats([[maybe_unused]] PixelFormat input)
{
	PixelFormat pixelFormat;
	std::vector<PixelFormat> pixelFormats;
	pixelFormats.push_back(pixelFormat.fromString("RGB888"));
	pixelFormats.push_back(pixelFormat.fromString("ARGB8888"));
	return pixelFormats;
}

SizeRange SimpleConverter::sizes([[maybe_unused]] const Size &input)
{
	SizeRange sizes;
	sizes.min = { 1, 1 };
	sizes.min = { UINT_MAX, UINT_MAX };
	return sizes;
}

std::tuple<unsigned int, unsigned int>
SimpleConverter::strideAndFrameSize([[maybe_unused]] const PixelFormat &pixelFormat, const Size &sz)
{
	PixelFormatInfo format;
	return std::make_tuple(format.stride(sz.width, 0, 1), format.frameSize(sz, 1));
}

int SimpleConverter::exportBuffers(unsigned int output, unsigned int count,
				   std::vector<std::unique_ptr<FrameBuffer>> *buffers)
{
	if (output != 0) {
		return -1;
	}
	if (outputBuffers.size() > 0) {
		return -1;
	}
	std::vector<std::unique_ptr<FrameBuffer>> out;
	for (unsigned i = 0; i < count; ++i) {
		auto tex = createBuffer();
		outputBuffers.emplace_back(tex.second);
		out.emplace_back(std::move(tex.first));
	}
	for (auto &buf : out) {
		buffers->push_back(std::move(buf));
	}
	return count;
}

std::pair<std::unique_ptr<FrameBuffer>, GlRenderTarget> SimpleConverter::createBuffer()
{
	bo = gbm_bo_create(gbm, outformat.size.width, outformat.size.height, GBM_BO_FORMAT_ARGB8888, GBM_BO_USE_RENDERING);
	unsigned int filedesc = gbm_bo_get_fd(bo);
	dmabuf_image dimg = import_dmabuf(filedesc, outformat.size, libcamera::formats::ARGB8888);

	std::vector<FrameBuffer::Plane> planes;
	UniqueFD fd(filedesc);
	FrameBuffer::Plane plane;
	plane.fd = SharedFD(std::move(fd));
	plane.offset = gbm_bo_get_offset(bo, 0);
	plane.length = gbm_bo_get_stride_for_plane(bo, 0) * outformat.size.height;

	planes.push_back(std::move(plane));

	auto fb = std::make_unique<FrameBuffer>(planes);
	fb->metadata_mut().planes()[0].bytesused = plane.length;
	return std::make_pair(std::move(fb), GlRenderTarget(fb.get(), dimg));
}

SimpleConverter::dmabuf_image SimpleConverter::import_dmabuf(int fdesc, Size pixelSize, libcamera::PixelFormat format)
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

	auto image = eglCreateImageKHR(
		dpy,
		EGL_NO_CONTEXT,
		EGL_LINUX_DMA_BUF_EXT,
		NULL,
		attrs);

	auto e = glGetError();
	if (e != GL_NO_ERROR) {
		std::cout << "GL_ERROR: " << e << std::endl;
	}
	GLuint texture;
	glGenTextures(1, &texture);
	struct dmabuf_image img = {
		.texture = texture,
		.image = image,
	};

	glBindTexture(GL_TEXTURE_2D, texture);
	//auto glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
	//glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);

	return img;
}

int SimpleConverter::start()
{
	assert(eglBindAPI(EGL_OPENGL_API) == EGL_TRUE);
	dev = open("/dev/dri/card0", O_RDWR); /*confirm*/
	gbm = gbm_create_device(dev);
	//struct gbm_surface *gbm_surf = gbm_surface_create(gbm, 1024, 1024, GBM_FORMAT_XRGB8888, GBM_BO_USE_RENDERING);

	auto eglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
	/* get an EGL display connection */
	dpy = eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_MESA, gbm, NULL);

	/* initialize the EGL display connection */
	eglInitialize(dpy, NULL, NULL);
	EGLConfig config;
	EGLint n_of_configs;
	assert(eglGetConfigs(dpy, &config, 1, &n_of_configs) == EGL_TRUE);

	//auto eglCreatePlatformWindowSurfaceEXT = (PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC)eglGetProcAddress("eglCreatePlatformWindowSurfaceEXT");
	/* create an EGL window surface */
	//srf = eglCreatePlatformWindowSurfaceEXT(dpy, config, gbm_surf, NULL);
	//assert(srf != EGL_NO_SURFACE);
	ctx = eglCreateContext(dpy, config, EGL_NO_CONTEXT, NULL);
	assert(ctx != EGL_NO_CONTEXT);

	/* connect the context to the surface */
	assert(eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx) == EGL_TRUE);

	/*Load GLEW so it configures OpenGL*/
	glewInit();

	shaderProgram.callShader("default.vert", "default.frag");
	framebufferProgram.callShader("bayer_8.vert", "bayer_8.frag");
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
	framebufferProgram.Activate();
	glBindAttribLocation(framebufferProgram.ID, 0, "vertexIn");
	glBindAttribLocation(framebufferProgram.ID, 2, "textureIn");
	glUniform1i(glGetUniformLocation(framebufferProgram.ID, "tex_y"), 0);
	glUniform2f(glGetUniformLocation(framebufferProgram.ID, "tex_step"), 1.0f / (informat.planes[0].bpl - 1),
		    1.0f / (informat.size.height - 1));
	glUniform2i(glGetUniformLocation(framebufferProgram.ID, "tex_size"), informat.size.width,
		    informat.size.height);
	glUniform2f(glGetUniformLocation(framebufferProgram.ID, "tex_bayer_first_red"), 0.0, 1.0);

	/* Prepare framebuffer rectangle VBO and VAO*/
	unsigned int rectVAO, rectVBO;
	glGenVertexArrays(1, &rectVAO);
	glGenBuffers(1, &rectVBO);
	glBindVertexArray(rectVAO);
	glBindBuffer(GL_ARRAY_BUFFER, rectVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(rectangleVertices), &rectangleVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));

	/* create FrameBuffer object*/

	glGenFramebuffers(1, &FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	dmabuf_image rend_tex = import_dmabuf(output->planes()[0].fd.get(), outformat.size, libcamera::formats::ARGB8888);

	Texture home(mappedBuffers_[input].get(), GL_TEXTURE_2D, GL_TEXTURE0, GL_LUMINANCE, GL_UNSIGNED_BYTE, informat.size, rend_tex.texture);

	/* Error checking framebuffer*/
	auto fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "Framebuffer error: " << fboStatus << std::endl;

	/* Main*/
	/* Bind the custom framebuffer*/
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	/* Specify the color of the background*/
	glClearColor(0.54f, 0.1f, 0.57f, 1.0f);
	/* Clean the back buffer and assign the new color to it*/
	glClear(GL_COLOR_BUFFER_BIT);
	/* Bind the default framebuffer*/
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	/* Draw the framebuffer rectangle*/
	framebufferProgram.Activate();
	glBindVertexArray(rectVAO);
	home.Bind();
	glDrawArrays(GL_TRIANGLES, 0, 6);

	return 0;
}

void SimpleConverter::stop()
{
	/*Delete all the objects we've created*/
	shaderProgram.Delete();
	glDeleteFramebuffers(1, &FBO);
	eglDestroySurface(dpy, srf);
	eglDestroyContext(dpy, ctx);
	eglTerminate(dpy);

	gbm_device_destroy(gbm);
	close(dev);
}

} /* namespace libcamera */