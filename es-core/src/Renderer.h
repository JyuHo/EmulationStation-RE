#ifndef _RENDERER_H_
#define _RENDERER_H_

#include <vector>
#include <string>
#include <span>
#include "platform.h"
#include "math/Math.h"
#include GLHEADER

class GuiComponent;
class Font;
struct SDL_Window;

//The Renderer provides several higher-level functions for drawing (rectangles, text, etc.).
//Renderer_draw_gl.cpp has most of the higher-level functions and wrappers.
//Renderer_init_sdlgl.cpp creates the SDL window and the OpenGL context.
namespace Renderer {

struct Vertex
{
	glm::vec2 pos;
	glm::vec2 tex;
	uint32_t color;
};
bool init(int w, int h);
void deinit();

unsigned int getScreenWidth();
unsigned int getScreenHeight();

// nullptr before init() and after deinit()
SDL_Window* getSDLWindow();

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

void drawTriangles(std::span<const Vertex> vertices, GLuint texture, GLenum blend_sfactor = GL_SRC_ALPHA,
	GLenum blend_dfactor = GL_ONE_MINUS_SRC_ALPHA);
void drawLines(std::span<const Vertex> vertices, GLenum blend_sfactor = GL_SRC_ALPHA,
	GLenum blend_dfactor = GL_ONE_MINUS_SRC_ALPHA);
extern GLuint shaderProgram;
extern GLuint vao;
extern GLuint vbo;
extern GLint uUseTexture;
extern GLint uTexture;
extern GLint uModel;
extern glm::mat4 projection;
extern glm::mat4 modelMatrix;

} // namespace Renderer

#endif
