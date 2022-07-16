#include "converter_gl.h"

#include <Texture.h>
#include <gbm.h>

#include <libcamera/base/unique_fd.h>

#include <libcamera/formats.h>
#include <libcamera/framebuffer.h>
#include <libcamera/stream.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <GL/glew.h>

#include "shaderClass.h"

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
			       const StreamConfiguration &outputCfg)
{
	informat.size = inputCfg.size;
	informat.planes[0].bpl = inputCfg.stride;
	outformat.size = outputCfg.size;
	outformat.planes[0].bpl = outputCfg.stride;
	// format.size = outputCfg.pixelFormat;
}

std::vector<std::unique_ptr<FrameBuffer>> SimpleConverter::exportBuffers(unsigned int count,
									 std::vector<std::unique_ptr<FrameBuffer>> *buffers)
{
	for (unsigned i = 0; i < count; ++i) {
		auto tex = createBuffer(i);
		outputBuffers.emplace_back(tex.second);
		buffers->push_back(std::move(tex.first));
	}
	return *buffers;
}

std::pair<std::unique_ptr<FrameBuffer>, GlRenderTarget> SimpleConverter::createBuffer(unsigned int index)
{
	bo = gbm_bo_create(gbm, outformat.size.width, outformat.size.height, GBM_BO_FORMAT_ARGB8888, GBM_BO_USE_RENDERING);
	unsigned int filedesc = gbm_bo_get_fd_for_plane(bo, 0);
	dmabuf_image dimg = import_dmabuf(filedesc, outformat.size, libcamera::formats::ARGB8888);

	// auto gltexture = dimg.texture;
	// struct tex text;
	// text.texture = dimg.texture;

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

SimpleConverter::dmabuf_image SimpleConverter::import_dmabuf(int fd, Size pixelSize, libcamera::PixelFormat format)
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
		fd,
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

	// glBindTexture(GL_TEXTURE_2D, texture);
	// auto glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
	// glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);

	return img;
}

void SimpleConverter::start()
{
	assert(eglBindAPI(EGL_OPENGL_API) == EGL_TRUE);
	fd = open("/dev/dri/card0", O_RDWR); /*confirm*/
	gbm = gbm_create_device(fd);
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
}

std::unique_ptr<FrameBuffer> SimpleConverter::queueBuffers(FrameBuffer *input, FrameBuffer *output)
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

	/* Gets ID of uniform called "scale"*/
	GLuint uniID = glGetUniformLocation(shaderProgram.ID, "scale");

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

	Texture home(mappedBuffers_[input].get(), GL_TEXTURE_2D, GL_TEXTURE0, GL_LUMINANCE, GL_UNSIGNED_BYTE);

	/* Create Render Buffer Object
	unsigned int RBO;
	glGenRenderbuffers(1, &RBO);
	glBindRenderbuffer(GL_RENDERBUFFER, RBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 700, 700);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, RBO);*/

	/* Error checking framebuffer*/
	auto fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "Framebuffer error: " << fboStatus << std::endl;

	/* Main*/
	/* Bind the custom framebuffer*/
	//glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	/* Specify the color of the background*/
	glClearColor(0.54f, 0.1f, 0.57f, 1.0f);
	/* Clean the back buffer and assign the new color to it*/
	glClear(GL_COLOR_BUFFER_BIT);
	/* Enable depth testing since it's disabled when drawing the framebuffer rectangle*/
	//glEnable(GL_DEPTH_TEST);
	/* Bind the default framebuffer*/
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	/* Draw the framebuffer rectangle*/
	framebufferProgram.Activate();
	glBindVertexArray(rectVAO);
	//glDisable(GL_DEPTH_TEST); /*prevents framebuffer rectangle from being discarded*/
	home.Bind();
	glDrawArrays(GL_TRIANGLES, 0, 6);

	return std::make_unique<FrameBuffer>(output);
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
	close(fd);
}

} /* namespace libcamera */