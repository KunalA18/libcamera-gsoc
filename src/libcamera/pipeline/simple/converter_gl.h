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
	int queueBuffer(FrameBuffer *input, FrameBuffer *output);
	void start();
	void stop();
	void exportBuffers(unsigned int count,
			   std::vector<std::unique_ptr<FrameBuffer>> *buffers);
	std::unique_ptr<FrameBuffer> createBuffer(unsigned int index);
	int configure(const StreamConfiguration &inputCfg,
		      const StreamConfiguration &outputCfg);
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

private:
	std::map<libcamera::FrameBuffer *, std::unique_ptr<MappedFrameBuffer>> mappedBuffers_;
	EGLDisplay dpy;
	EGLSurface srf;
	EGLContext ctx;
	int fd;
	struct gbm_device *gbm;
	struct gbm_bo *bo;
};

} /* namespace libcamera */