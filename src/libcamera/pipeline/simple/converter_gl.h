#pragma once

#include <libcamera/base/log.h>

namespace libcamera {

class FrameBuffer;

class SimpleConverter
{
public:
	int queueBuffer(FrameBuffer *input);

private:
};

} /* namespace libcamera */