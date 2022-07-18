#ifndef SHADER_CLASS_H
#define SHADER_CLASS_H

#include <cerrno>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string.h>

#include <GL/glew.h>

std::string get_file_contents(const char *filename);

class Shader
{
public:
	// Reference ID of the Shader Program
	GLuint ID;
	// Constructor that build the Shader Program from 2 different shaders
	void callShader(const char *vertexFile, const char *fragmentFile);

	// Activates the Shader Program
	void Activate();
	// Deletes the Shader Program
	void Delete();

private:
	// Checks if the different Shaders have compiled properly
	void compileErrors(unsigned int shader, const char *type);
};
#endif