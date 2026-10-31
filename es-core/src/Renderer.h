#ifndef _RENDERER_H_
#define _RENDERER_H_

#include <vector>
#include <string>
#include "platform.h"
#include "math/Math.h"
#include GLHEADER

class GuiComponent;
class Font;

//The Renderer provides several higher-level functions for drawing (rectangles, text, etc.).
//Renderer_draw_gl.cpp has most of the higher-level functions and wrappers.
//Renderer_init_sdlgl.cpp creates the SDL window and the OpenGL context.
namespace Renderer {
bool init(int w, int h);
void deinit();

unsigned int getScreenWidth();
unsigned int getScreenHeight();

void buildGLColorArray(GLubyte* ptr, unsigned int color, unsigned int vertCount);

//graphics commands
void swapBuffers();

void pushClipRect(glm::ivec2 pos, glm::ivec2 dim);
void popClipRect();

void setMatrix(float* mat);
void setMatrix(const glm::mat4& transform);

void drawRect(int x, int y, int w, int h, unsigned int color, GLenum blend_sfactor = GL_SRC_ALPHA,
	GLenum blend_dfactor = GL_ONE_MINUS_SRC_ALPHA);
void drawRect(float x, float y, float w, float h, unsigned int color, GLenum blend_sfactor = GL_SRC_ALPHA,
	GLenum blend_dfactor = GL_ONE_MINUS_SRC_ALPHA);
} // namespace Renderer

#endif
