/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Copyright (C) 2022, Kunal Agarwal
 *
 * converter_gl.cpp - GL converter for debayering
 */

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

#include "libcamera/internal/mapped_framebuffer.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "shader.h"

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
	struct DmabufImage {
		GLuint texture;
		EGLImageKHR image;
	};

private:
	int configureGL(const StreamConfiguration &inputCfg,
			const StreamConfiguration &outputCfg);
	DmabufImage importDmabuf(int fdesc, Size pixelSize, PixelFormat format);
	int queueBufferGL(FrameBuffer *input, FrameBuffer *output);

	std::map<libcamera::FrameBuffer *, std::unique_ptr<MappedFrameBuffer>>
		mappedBuffers_;

	struct ConverterFormat {
		struct Plane {
			uint32_t size_ = 0;
			uint32_t bpl_ = 0;
		};
		PixelFormat fourcc;
		Size size;
		std::array<Plane, 3> planes;
		unsigned int planesCount = 0;
	};

	int device_;
	unsigned int rectVAO, rectVBO;
	EGLDisplay display_;
	EGLContext context_;

	struct gbm_device *gbm;
	struct gbm_bo *bo;
	unsigned int fbo_;

	ConverterFormat informat_;
	ConverterFormat outformat_;
	ShaderProgram shaderProgram_;
	ShaderProgram framebufferProgram_;
	std::vector<GlRenderTarget> outputBuffers;
};

class GlRenderTarget
{
public:
	struct SimpleConverter::DmabufImage texture_;

	/* This is never to be dereferenced. Only serves for comparison */
	const FrameBuffer *buffer_;

	GlRenderTarget(FrameBuffer *buffer, struct SimpleConverter::DmabufImage texture)
		: texture_(texture), buffer_(buffer)
	{
	}
};

} /* namespace libcamera */
