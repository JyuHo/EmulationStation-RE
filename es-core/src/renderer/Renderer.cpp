#include "platform.h"
#include "renderer/Renderer.h"
#include GLHEADER
#include <iostream>
#include "resources/Font.h"
#include <filesystem>
#include "Log.h"
#include <stack>
#include "Util.h"
#include <cstddef>
#include <cstring>

namespace Renderer {
std::stack<glm::ivec4> clipStack;

void setColor4bArray(GLubyte* array, unsigned int color)
{
	array[0] = (color & 0xff000000) >> 24;
	array[1] = (color & 0x00ff0000) >> 16;
	array[2] = (color & 0x0000ff00) >> 8;
	array[3] = (color & 0x000000ff);
}

void buildGLColorArray(GLubyte* ptr, unsigned int color, unsigned int vertCount)
{
	unsigned int colorGl;
	setColor4bArray((GLubyte*)&colorGl, color);
	for(unsigned int i = 0; i < vertCount; i++)
	{
		((GLuint*)ptr)[i] = colorGl;
	}
}

void pushClipRect(glm::ivec2 pos, glm::ivec2 dim)
{
	glm::ivec4 box(pos.x, pos.y, dim.x, dim.y);
	if(box[2] == 0)
		box[2] = Renderer::getScreenWidth() - box.x;
	if(box[3] == 0)
		box[3] = Renderer::getScreenHeight() - box.y;

	box[1] = Renderer::getScreenHeight() - box.y - box[3];

	if(clipStack.size())
	{
		glm::ivec4& top = clipStack.top();
		if(top[0] > box[0])
			box[0] = top[0];
		if(top[1] > box[1])
			box[1] = top[1];
		if(top[0] + top[2] < box[0] + box[2])
			box[2] = (top[0] + top[2]) - box[0];
		if(top[1] + top[3] < box[1] + box[3])
			box[3] = (top[1] + top[3]) - box[1];
	}

	if(box[2] < 0)
		box[2] = 0;
	if(box[3] < 0)
		box[3] = 0;

	clipStack.push(box);
	glScissor(box[0], box[1], box[2], box[3]);
	glEnable(GL_SCISSOR_TEST);
}

void popClipRect()
{
	if(clipStack.empty())
	{
		LOG(LogError) << "Tried to popClipRect while the stack was empty!";
		return;
	}

	clipStack.pop();
	if(clipStack.empty())
	{
		glDisable(GL_SCISSOR_TEST);
	}
	else
	{
		glm::ivec4 top = clipStack.top();
		glScissor(top[0], top[1], top[2], top[3]);
	}
}

void drawRect(float x, float y, float w, float h, unsigned int color, GLenum blend_sfactor, GLenum blend_dfactor)
{
	drawRect((int)round(x), (int)round(y), (int)round(w), (int)round(h), color, blend_sfactor, blend_dfactor);
}

void drawRect(int x, int y, int w, int h, unsigned int color, GLenum blend_sfactor, GLenum blend_dfactor)
{
	Vertex verts[6];
	// color as RGBA in uint32; shader expects 4 bytes. Store as ABGR? We'll pack RGBA into uint32 and read as unsigned byte with GL_TRUE? Vertex color attribute is 4 unsigned bytes. Need consistent packing.
	// Pack color (0xAABBGGRR? original color format). Original buildGLColorArray stored color bytes [A?]. Let's keep same: color passed is 0xRRGGBBAA? Check: setColor4bArray sets array[0]=alpha? Actually original: array[0]=(color&0xff000000)>>24. So color high byte -> array[0]. OpenGL color pointer GL_UNSIGNED_BYTE. We'll pack into uint32 with same byte order (memory). Use color directly as the GLuint value? The original did ((GLuint*)ptr)[i]=colorGl where colorGl had bytes set via setColor4bArray into (GLubyte*)&colorGl. So memory layout [byte0,byte1,byte2,byte3] = [high..low]. Our uint32_t color field read as 4 unsigned bytes (GL_TRUE normalize) in vertex attrib. If we store the same integer, memory same. So verts color = color.
	uint32_t c = color;
	verts[0] = { { (float)x, (float)y }, {0,0}, c };
	verts[1] = { { (float)x, (float)(y + h) }, {0,0}, c };
	verts[2] = { { (float)(x + w), (float)y }, {0,0}, c };
	verts[3] = { { (float)(x + w), (float)y }, {0,0}, c };
	verts[4] = { { (float)x, (float)(y + h) }, {0,0}, c };
	verts[5] = { { (float)(x + w), (float)(y + h) }, {0,0}, c };

	drawTriangles(std::span<const Vertex>(verts, 6), 0, blend_sfactor, blend_dfactor);
}

void setMatrix(float* matrix)
{
	modelMatrix = glm::mat4(1.0f);
	std::memcpy(glm::value_ptr(modelMatrix), matrix, sizeof(float) * 16);
	glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(modelMatrix));
}

void setMatrix(const glm::mat4& matrix)
{
	modelMatrix = matrix;
	glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(modelMatrix));
}

void drawTriangles(std::span<const Vertex> vertices, GLuint texture, GLenum blend_sfactor, GLenum blend_dfactor)
{
	if(vertices.empty())
		return;

	glUseProgram(shaderProgram);
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_DYNAMIC_DRAW);

	glBlendFunc(blend_sfactor, blend_dfactor);
	glEnable(GL_BLEND);

	if(texture != 0)
	{
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture);
		glUniform1i(uUseTexture, 1);
	}
	else
	{
		glUniform1i(uUseTexture, 0);
	}

	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertices.size());

	if(texture != 0)
		glBindTexture(GL_TEXTURE_2D, 0);
}

void drawLines(std::span<const Vertex> vertices, GLenum blend_sfactor, GLenum blend_dfactor)
{
	if(vertices.empty())
		return;

	glUseProgram(shaderProgram);
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_DYNAMIC_DRAW);

	glUniform1i(uUseTexture, 0);
	glBlendFunc(blend_sfactor, blend_dfactor);
	glEnable(GL_BLEND);

	glDrawArrays(GL_LINES, 0, (GLsizei)vertices.size());
}

} // namespace Renderer