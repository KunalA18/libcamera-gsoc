#pragma once

#include <assert.h>
#include <fcntl.h>
#include <map>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>

#include <libcamera/base/log.h>

#include <cam/image.h>

namespace libcamera {

class FrameBuffer;

class SimpleConverter
{
public:
	int queueBuffer(FrameBuffer *input);

private:
	std::map<libcamera::FrameBuffer *, std::unique_ptr<Image>> mappedBuffers_;
};

} /* namespace libcamera */