/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Copyright (C) 2022, Kunal Agarwal
 *
 * shader.cpp - Shader Handling
 */

#include "shader.h"

#include <libcamera/base/file.h>
#include <libcamera/base/log.h>

#include <GLES3/gl3.h>

namespace libcamera {

LOG_DECLARE_CATEGORY(SimplePipeline)

/* Reads a text file and outputs a string with everything in the text file */
static std::string get_file_contents(const char *filename)
{
	std::string fullname = std::string("/home/pi/Desktop/compile/libcamera/src/libcamera/pipeline/simple/shader/") + filename;

	File file(fullname);
	if (!file.open(File::OpenModeFlag::ReadOnly))
		return "";

	Span<uint8_t> data = file.map();
	return std::string(reinterpret_cast<char *>(data.data()), data.size());
}

/* Constructor that build the Shader Program from 2 different shaders */
void ShaderProgram::callShader(const char *vertexFile, const char *fragmentFile)
{
	/* Read vertexFile and fragmentFile and store the strings */
	std::string vertexCode = get_file_contents(vertexFile);
	std::string fragmentCode = get_file_contents(fragmentFile);
	const char *vertexSource = vertexCode.c_str();
	const char *fragmentSource = fragmentCode.c_str();

	/* Create the vertex shader, set its source code and compile it. */
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexSource, NULL);
	glCompileShader(vertexShader);
	compileErrors(vertexShader, "VERTEX");

	/* Create the fragment shader, set its source code and compile it. */
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
	glCompileShader(fragmentShader);
	compileErrors(fragmentShader, "FRAGMENT");

	/* Create Shader Program Object and get its reference */
	id_ = glCreateProgram();

	/* Attach and wrap-up/link the Vertex and Fragment Shaders to the Shader Program */
	glAttachShader(id_, vertexShader);
	glAttachShader(id_, fragmentShader);
	glLinkProgram(id_);

	/* Checks if Shaders linked succesfully */
	compileErrors(id_, "PROGRAM");

	/* Delete the Vertex and Fragment Shader objects. Here, they are flagged for deletion
	   and will not be deleted until they are detached from the program object. This frees
	   up the memory used to store the shader source.
	*/
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
}

/* Activates the Shader Program */
void ShaderProgram::activate()
{
	glUseProgram(id_);
}

/* Deletes the Shader Program */
void ShaderProgram::deleteProgram()
{
	glDeleteProgram(id_);
}

/* Checks if the different Shaders have compiled properly */
void ShaderProgram::compileErrors(unsigned int shader, const char *type)
{
	/* Stores status of compilation */
	GLint hasCompiled;
	GLint logLength = 1024;
	/* Character array to store error message in */

	if (strcmp(type, "PROGRAM") != 0) {
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
		char *infoLog = new char[logLength];
		glGetShaderiv(shader, GL_COMPILE_STATUS, &hasCompiled);
		if (hasCompiled == GL_FALSE) {
			glGetShaderInfoLog(shader, logLength, NULL, infoLog);
			LOG(SimplePipeline, Error) << "SHADER_COMPILATION_ERROR for:"
						   << type << "\t"
						   << infoLog;
		}

	} else {
		glGetProgramiv(shader, GL_INFO_LOG_LENGTH, &logLength);
		char *infoLog = new char[logLength];
		glGetProgramiv(shader, GL_LINK_STATUS, &hasCompiled);
		if (hasCompiled == GL_FALSE) {
			glGetProgramInfoLog(shader, logLength, NULL, infoLog);
			LOG(SimplePipeline, Error) << "SHADER_LINKING_ERROR for:"
						   << type << "\t"
						   << infoLog;
		}
		int e = glGetError();
		if (e != GL_NO_ERROR)
			LOG(SimplePipeline, Error) << "GL_ERROR: " << e;
	}
}
} /* namespace libcamera */
