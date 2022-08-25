/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Copyright (C) 2022, Kunal Agarwal
 *
 * shader.h - Shader Handling
 */

#pragma once

#include <iostream>
#include <string.h>

#include <GL/gl.h>

namespace libcamera {

class ShaderProgram
{
public:
	void callShader(const char *vertexFile, const char *fragmentFile);

	void activate();

	void deleteProgram();

	int id() const { return id_; };

private:
	/* Reference ID of the Shader Program */
	GLuint id_;
	void compileErrors(unsigned int shader, const char *type);
};

} /* namespace libcamera */
