/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Copyright (C) 2022, Kunal Agarwal
 *
 * texture.cpp - Texture Handling
 */

#include "texture.h"

#include <libcamera/framebuffer.h>

#include <GLES3/gl3.h>

namespace libcamera {
void Texture::initTexture(GLenum slot)
{
	/* Generates an OpenGL texture object and assigns the texture to a Texture Unit  */
	glGenTextures(1, &idTex_);
	glActiveTexture(slot);
	glBindTexture(type_, idTex_);

	/* Configures the type of algorithm that is used to make the image smaller or bigger */
	glTexParameteri(type_, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(type_, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	/* Prevents edge bleeding */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void Texture::startTexture(MappedFrameBuffer *image, GLenum format, GLenum pixelType, Size pixelSize)
{
	/* Assigns the image to the OpenGL Texture object */
	glTexImage2D(type_, 0, GL_LUMINANCE, pixelSize.width, pixelSize.height, 0, format, pixelType, image->planes()[0].data());
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, idTex_, 0);
}

void Texture::bind()
{
	glBindTexture(type_, idTex_);
}

void Texture::unbind()
{
	glBindTexture(type_, 0);
}

void Texture::deleteText()
{
	glDeleteTextures(1, &idTex_);
}

} /* namespace libcamera */
