/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Copyright (C) 2022, Kunal Agarwal
 *
 * texture.h - Texture Handling
 */

#pragma once

#include <libcamera/base/log.h>

#include <libcamera/geometry.h>

#include "libcamera/internal/mapped_framebuffer.h"

#include "shader.h"

namespace libcamera {

class FrameBuffer;

class Texture
{
public:
	GLuint idTex_;
	GLenum type_;

	Texture(GLenum texType, GLuint rend_text)
		: idTex_(rend_text), type_(texType){};

	//void startTexture(const MappedBuffer::Plane *image, GLenum format, GLenum pixelType, Size pixelSize);
	void startTexture();

	void texUnit(ShaderProgram &shader, const char *uniform, GLuint unit);

	void bind();

	void unbind();

	void deleteText();
};

} /* namespace libcamera */
