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
	int queueBuffer(FrameBuffer *input);
	void start();
	void stop();
	void SimpleConverter::exportBuffers(unsigned int count,
					    std::vector<std::unique_ptr<FrameBuffer>> *buffers);

private:
	std::map<libcamera::FrameBuffer *, std::unique_ptr<MappedFrameBuffer>> mappedBuffers_;
	EGLDisplay dpy;
	EGLSurface srf;
	EGLContext ctx;
	int fd;
	struct gbm_device *gbm;
};

} /* namespace libcamera */