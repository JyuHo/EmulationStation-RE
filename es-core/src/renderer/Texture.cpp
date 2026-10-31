#include "renderer/GL.h"
#include "renderer/Renderer.h"

namespace Renderer {

namespace {

struct FormatInfo
{
	GLint internalFormat;
	GLenum format;
};

FormatInfo formatInfo(TextureFormat format)
{
	switch(format)
	{
	case TextureFormat::Alpha8: return {GL_R8, GL_RED};
	case TextureFormat::RGBA8: break;
	}
	return {GL_RGBA8, GL_RGBA};
}

} // namespace

TextureHandle createTexture(TextureFormat format, TextureFilter filter, bool repeat, glm::ivec2 size, const void* data)
{
	GLuint texture = 0;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	// MIN and MAG are always set: the default MIN filter uses mipmaps, and a texture without
	// them would be incomplete (sampled as black)
	const GLint minFilter = filter == TextureFilter::Nearest ? GL_NEAREST : GL_LINEAR;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	const GLint wrap = repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);

	if(format == TextureFormat::Alpha8)
	{
		// sampled as (1, 1, 1, r): colour x texel gives the vertex colour with the glyph's alpha,
		// as GL_ALPHA + GL_MODULATE did in the fixed pipeline
		const GLint swizzle[] = {GL_ONE, GL_ONE, GL_ONE, GL_RED};
		glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);
	}

	const FormatInfo info = formatInfo(format);
	glTexImage2D(GL_TEXTURE_2D, 0, info.internalFormat, size.x, size.y, 0, info.format, GL_UNSIGNED_BYTE, data);

	glBindTexture(GL_TEXTURE_2D, 0);
	return texture;
}

void updateTexture(TextureHandle texture, TextureFormat format, glm::ivec2 pos, glm::ivec2 size, const void* data)
{
	if(texture == 0)
		return;

	const FormatInfo info = formatInfo(format);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, pos.x, pos.y, size.x, size.y, info.format, GL_UNSIGNED_BYTE, data);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void destroyTexture(TextureHandle texture)
{
	if(texture == 0)
		return;

	const GLuint id = texture;
	glDeleteTextures(1, &id);
}

} // namespace Renderer
