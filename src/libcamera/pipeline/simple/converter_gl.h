#pragma once

#include <assert.h>
#include <fcntl.h>
#include <map>
#include <mapped_framebuffer.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>

#include <libcamera/base/log.h>

namespace libcamera {

class FrameBuffer;

class SimpleConverter
{
public:
	std::unique_ptr<FrameBuffer> queueBuffers(FrameBuffer *input, FrameBuffer *output);
	void start();
	void stop();
	void exportBuffers(unsigned int count,
			   std::vector<std::unique_ptr<FrameBuffer>> *buffers);
	std::pair<std::unique_ptr<FrameBuffer>, GlRenderTarget> createBuffer(unsigned int index);
	int configure(const StreamConfiguration &inputCfg,
		      const StreamConfiguration &outputCfg);
	struct dmabuf_image {
		GLuint texture;
		EGLImageKHR image;
	};
	dmabuf_image import_dmabuf(int fd, Size pixelSize, libcamera::PixelFormat format);
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
	// struct tex {
	// 	int dmafd;
	// 	dmabuf_image texture;
	// };

private:
	std::map<libcamera::FrameBuffer *, std::unique_ptr<MappedFrameBuffer>>
		mappedBuffers_;
	EGLDisplay dpy;
	EGLSurface srf;
	EGLContext ctx;
	int fd;
	struct gbm_device *gbm;
	struct gbm_bo *bo;
	unsigned int FBO;
};

class GlRenderTarget : public SimpleConverter
{
public:
	struct dmabuf_image texture;

	//private:
	// This is never to be dereferenced. Only serves for comparison
	const FrameBuffer *buffer;

	GlRenderTarget(FrameBuffer *buffer_, struct dmabuf_image texture_)
		: texture(texture_),
		  buffer(buffer_)
	{
	}
};

} /* namespace libcamera */