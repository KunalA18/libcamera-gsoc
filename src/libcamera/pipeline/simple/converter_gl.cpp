#include "converter_gl.h"

#include <Texture.h>
#include <gbm.h>

#include <libcamera/framebuffer.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglplatform.h>
#include <GL/glew.h>
#include <cam/image.h>

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

int queueBuffer(FrameBuffer *input)
{
	assert(eglBindAPI(EGL_OPENGL_API) == EGL_TRUE);
	int fd = open("/dev/dri/card0", O_RDWR); /*confirm*/
	struct gbm_device *gbm = gbm_create_device(fd);
	struct gbm_surface *gbm_surf = gbm_surface_create(gbm, 256, 256, GBM_FORMAT_XRGB8888, GBM_BO_USE_RENDERING);

	/* get an EGL display connection */
	EGLDisplay dpy = eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_MESA, gbm, NULL);

	/* initialize the EGL display connection */
	eglInitialize(dpy, NULL, NULL);
	EGLConfig config;
	EGLint n_of_configs;
	assert(eglGetConfigs(dpy, &config, 1, &n_of_configs) == EGL_TRUE);

	/* create an EGL window surface */
	EGLSurface srf = eglCreatePlatformWindowSurfaceEXT(dpy, config, gbm_surf, NULL);
	assert(srf != EGL_NO_SURFACE);
	EGLContext ctx = eglCreateContext(dpy, config, EGL_NO_CONTEXT, NULL);
	assert(ctx != EGL_NO_CONTEXT);

	/* connect the context to the surface */
	assert(eglMakeCurrent(dpy, srf, srf, ctx) == EGL_TRUE);

	//Load GLEW so it configures OpenGL
	glewInit();

	// Specify the viewport of OpenGL in the Window
	// In this case the viewport goes from x = 0, y = 0, to x = 700, y = 700
	//glViewport(0, 0, 700, 700);

	Shader shaderProgram("default.vert", "default.frag");
	Shader framebufferProgram("bayer_8.vert", "bayer_8.frag");

	framebufferProgram.Activate();
	glUniform1i(glGetUniformLocation(framebufferProgram.ID, "screenTexture"), 0);

	// Gets ID of uniform called "scale"
	GLuint uniID = glGetUniformLocation(shaderProgram.ID, "scale");

	// Prepare framebuffer rectangle VBO and VAO
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

	// create FrameBuffer object
	unsigned int FBO;
	glGenFramebuffers(1, &FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);

	Texture home(mappedBuffers_[input].get(), GL_TEXTURE_2D, GL_TEXTURE0, GL_LUMINANCE, GL_UNSIGNED_BYTE);
	//home.texUnit(shaderProgram, "tex0", 0);

	// Create Render Buffer Object
	unsigned int RBO;
	glGenRenderbuffers(1, &RBO);
	glBindRenderbuffer(GL_RENDERBUFFER, RBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 700, 700);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, RBO);

	// Error checking framebuffer
	auto fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "Framebuffer error: " << fboStatus << std::endl;

	// Main while loop
	//while (!glfwWindowShouldClose(window)) {
	// Bind the custom framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	// Specify the color of the background
	glClearColor(0.54f, 0.1f, 0.57f, 1.0f);
	// Clean the back buffer and assign the new color to it
	glClear(GL_COLOR_BUFFER_BIT);
	// Enable depth testing since it's disabled when drawing the framebuffer rectangle
	glEnable(GL_DEPTH_TEST);
	// Bind the default framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	// Draw the framebuffer rectangle
	framebufferProgram.Activate();
	glBindVertexArray(rectVAO);
	glDisable(GL_DEPTH_TEST); // prevents framebuffer rectangle from being discarded
	home.Bind();
	glDrawArrays(GL_TRIANGLES, 0, 6);
	//}

	// Delete all the objects we've created
	shaderProgram.Delete();
	glDeleteFramebuffers(1, &FBO);

	eglDestroySurface(dpy, srf);
	eglDestroyContext(dpy, ctx);
	eglTerminate(dpy);

	gbm_device_destroy(gbm);
	close(fd);

	return EXIT_SUCCESS;

	/*code to return the output buffer*/
}

} /* namespace libcamera */