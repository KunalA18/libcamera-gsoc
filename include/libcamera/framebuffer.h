/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Copyright (C) 2019, Google Inc.
 *
 * framebuffer.h - Frame buffer handling
 */

#pragma once

#include <assert.h>
#include <limits>
#include <memory>
#include <stdint.h>
#include <vector>

#include <libcamera/base/class.h>
#include <libcamera/base/shared_fd.h>
#include <libcamera/base/span.h>

namespace libcamera {

class Fence;
class Request;

struct FrameMetadata {
	enum Status {
		FrameSuccess,
		FrameError,
		FrameCancelled,
	};

	struct Plane {
		unsigned int bytesused;
	};

	Status status;
	unsigned int sequence;
	uint64_t timestamp;

	Span<Plane> planes() { return planes_; }
	Span<const Plane> planes() const { return planes_; }

private:
	friend class FrameBuffer;

	std::vector<Plane> planes_;
};

class FrameBuffer final : public Extensible
{
	LIBCAMERA_DECLARE_PRIVATE()

public:
	struct Plane {
		static constexpr unsigned int kInvalidOffset = std::numeric_limits<unsigned int>::max();
		SharedFD fd;
		unsigned int offset = kInvalidOffset;
		unsigned int length;
	};

	FrameBuffer(const std::vector<Plane> &planes, unsigned int cookie = 0);
	FrameBuffer(std::unique_ptr<Private> d);

	const std::vector<Plane> &planes() const;
	Request *request() const;
<<<<<<< HEAD
	const FrameMetadata &metadata() const { return metadata_; }
	FrameMetadata &metadata_mut() { return metadata_; }
=======
	const FrameMetadata &metadata() const;
>>>>>>> ea8ae5afff226f9373c82c1a3185e532d5d6eda0

	uint64_t cookie() const;
	void setCookie(uint64_t cookie);

	std::unique_ptr<Fence> releaseFence();

private:
	LIBCAMERA_DISABLE_COPY_AND_MOVE(FrameBuffer)
};

} /* namespace libcamera */
