#ifndef TEXTURE_CLASS_H
#define TEXTURE_CLASS_H

#include <libcamera/geometry.h>

#include <GL/glew.h>
#include <libcamera/internal/mapped_framebuffer.h>

#include "shaderClass.h"

namespace libcamera {

class FrameBuffer;

class Texture
{
public:
	GLuint ID;
	GLenum type;
	Texture(MappedFrameBuffer *image, GLenum texType, GLenum slot, GLenum format, GLenum pixelType, Size Pixelsize);

	// Assigns a texture unit to a texture
	void texUnit(Shader &shader, const char *uniform, GLuint unit);
	// Binds a texture
	void Bind();
	// Unbinds a texture
	void Unbind();
	// Deletes a texture
	void Delete();
};
} // namespace libcamera
#endif