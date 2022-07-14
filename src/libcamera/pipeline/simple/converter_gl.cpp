#include "converter_gl.h"

#include <Texture.h>
#include <gbm.h>

#include <libcamera/framebuffer.h>
#include <libcamera/stream.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglplatform.h>
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

void SimpleConverter::start()
{
	assert(eglBindAPI(EGL_OPENGL_API) == EGL_TRUE);
	fd = open("/dev/dri/card0", O_RDWR); /*confirm*/
	gbm = gbm_create_device(fd);
	struct gbm_surface *gbm_surf = gbm_surface_create(gbm, 256, 256, GBM_FORMAT_XRGB8888, GBM_BO_USE_RENDERING);

	auto eglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
	/* get an EGL display connection */
	dpy = eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_MESA, gbm, NULL);

	/* initialize the EGL display connection */
	eglInitialize(dpy, NULL, NULL);
	EGLConfig config;
	EGLint n_of_configs;
	assert(eglGetConfigs(dpy, &config, 1, &n_of_configs) == EGL_TRUE);

	auto eglCreatePlatformWindowSurfaceEXT = (PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC)eglGetProcAddress("eglCreatePlatformWindowSurfaceEXT");
	/* create an EGL window surface */
	srf = eglCreatePlatformWindowSurfaceEXT(dpy, config, gbm_surf, NULL);
	assert(srf != EGL_NO_SURFACE);
	ctx = eglCreateContext(dpy, config, EGL_NO_CONTEXT, NULL);
	assert(ctx != EGL_NO_CONTEXT);

	/* connect the context to the surface */
	assert(eglMakeCurrent(dpy, srf, srf, ctx) == EGL_TRUE);

	/*Load GLEW so it configures OpenGL*/
	glewInit();
}

int SimpleConverter::configure(const StreamConfiguration &inputCfg,
			       const StreamConfiguration &outputCfg)
{
	informat.size = inputCfg.size;
	informat.planes[0].bpl = inputCfg.stride;
	outformat.size = outputCfg.size;
	outformat.planes[0].bpl = outputCfg.stride;
	// format.size = outputCfg.pixelFormat;
}

void SimpleConverter::exportBuffers(unsigned int count,
				    std::vector<std::unique_ptr<FrameBuffer>> *buffers)
{
	for (unsigned i = 0; i < count; ++i) {
		std::unique_ptr<FrameBuffer> buffer = createBuffer(i);
		buffers->push_back(std::move(buffer));
	}
}

std::unique_ptr<FrameBuffer> SimpleConverter::createBuffer(unsigned int index)
{
	bo = gbm_bo_create(gbm, outformat.size.width, outformat.size.height, GBM_BO_FORMAT_ARGB8888, GBM_BO_USE_RENDERING); /*confirm width,height,format*/

	std::vector<FrameBuffer::Plane> planes;
	for (unsigned int nplane = 0; nplane < 1; nplane++) {
		unsigned int filedesc = gbm_bo_get_fd_for_plane(bo, nplane);
		FrameBuffer::Plane plane;
		plane.fd = SharedFD(std::move(filedesc));
		plane.offset = gbm_bo_get_offset(bo, nplane);
		plane.length = gbm_bo_get_stride_for_plane(bo, nplane) * outformat.size.height;

		planes.push_back(std::move(plane));
	}
	return std::make_unique<FrameBuffer>(planes);
}

int SimpleConverter::queueBuffer(FrameBuffer *input, FrameBuffer *output)
{
	Shader shaderProgram("default.vert", "default.frag");
	Shader framebufferProgram("bayer_8.vert", "bayer_8.frag");

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
	unsigned int FBO;
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

	/*code to return the output buffer*/

	/*Delete all the objects we've created*/
	shaderProgram.Delete();
	glDeleteFramebuffers(1, &FBO);
}

void SimpleConverter::stop()
{
	eglDestroySurface(dpy, srf);
	eglDestroyContext(dpy, ctx);
	eglTerminate(dpy);

	gbm_device_destroy(gbm);
	close(fd);
}

} /* namespace libcamera */