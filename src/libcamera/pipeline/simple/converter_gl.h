#pragma once

#include <assert.h>
#include <fcntl.h>
#include <gbm.h>
#include <map>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>

#include <libcamera/base/log.h>
#include <libcamera/base/signal.h>

#include <libcamera/geometry.h>
#include <libcamera/stream.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <libcamera/internal/mapped_framebuffer.h>

#include "shaderClass.h"

namespace libcamera {

class FrameBuffer;
class GlRenderTarget;

class SimpleConverter
{
public:
	int configure(const StreamConfiguration &inputCfg,
		      const std::vector<std::reference_wrapper<StreamConfiguration>> &outputCfgs);
	std::vector<PixelFormat> formats(PixelFormat input);
	SizeRange sizes(const Size &input);

	std::tuple<unsigned int, unsigned int>
	strideAndFrameSize(const PixelFormat &pixelFormat, const Size &size);

	int queueBuffers(FrameBuffer *input,
			 const std::map<unsigned int, FrameBuffer *> &outputs);

	int start();
	void stop();

	int exportBuffers(unsigned int output, unsigned int count,
			  std::vector<std::unique_ptr<FrameBuffer>> *buffers);
	std::pair<std::unique_ptr<FrameBuffer>, GlRenderTarget> createBuffer();
	bool isValid() const { return true; }

	Signal<FrameBuffer *> inputBufferReady;
	Signal<FrameBuffer *> outputBufferReady;
	struct dmabuf_image {
		GLuint texture;
		EGLImageKHR image;
	};

private:
	int configureGL(const StreamConfiguration &inputCfg,
			const StreamConfiguration &outputCfg);
	dmabuf_image import_dmabuf(int fdesc, Size pixelSize, libcamera::PixelFormat format);
	int queueBufferGL(FrameBuffer *input, FrameBuffer *output);

	std::map<libcamera::FrameBuffer *, std::unique_ptr<MappedFrameBuffer>>
		mappedBuffers_;
	EGLDisplay dpy;
	EGLSurface srf;
	EGLContext ctx;
	int dev;
	struct gbm_device *gbm;
	struct gbm_bo *bo;
	unsigned int FBO;

	struct converterFormat {
		struct Plane {
			uint32_t size = 0;
			uint32_t bpl = 0;
		};
		Size size;
		std::array<Plane, 3> planes;
		unsigned int planesCount = 0;
	};
	converterFormat informat;
	converterFormat outformat;
	Shader shaderProgram;
	Shader framebufferProgram;
	std::vector<GlRenderTarget> outputBuffers;
};

class GlRenderTarget
{
public:
	struct SimpleConverter::dmabuf_image texture;

	// This is never to be dereferenced. Only serves for comparison
	const FrameBuffer *buffer;

	GlRenderTarget(FrameBuffer *buffer_, struct SimpleConverter::dmabuf_image texture_)
		: texture(texture_),
		  buffer(buffer_)
	{
	}
};

} /* namespace libcamera */