#include "renderer/Renderer.h"

#include "Log.h"
#include "renderer/GL.h"
#include "renderer/Internal.h"
#include "renderer/Shader.h"
#include "renderer/Shaders.h"

#include <cmath>
#include <cstddef>
#include <stack>

namespace Renderer {

namespace {

std::stack<glm::ivec4> clipStack;

Shader textureShader; // images, fonts
Shader colorShader;	  // rectangles, lines
GLuint vao = 0;
GLuint vbo = 0;

glm::mat4 modelView(1.0f);

// between initDrawing and deinitDrawing: a context exists and the GL functions are loaded
bool ready = false;

void applyBlend(BlendMode blend)
{
	switch(blend)
	{
	case BlendMode::Additive: glBlendFunc(GL_ONE, GL_ONE); return;
	case BlendMode::InvertDst: glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO); return;
	case BlendMode::Alpha: break;
	}
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

// one glDrawArrays per call, in call order (no batching yet)
void draw(GLenum primitive, std::span<const Vertex> vertices, TextureHandle texture, BlendMode blend)
{
	if(vertices.empty() || !ready)
		return;

	const Shader& shader = texture != 0 ? textureShader : colorShader;
	shader.use();
	// the last matrix set stays valid until the next setMatrix (IList relies on it)
	shader.setModelView(modelView);

	if(texture != 0)
		glBindTexture(GL_TEXTURE_2D, texture);

	applyBlend(blend);

	// orphan the previous storage, then upload
	const GLsizeiptr bytes = static_cast<GLsizeiptr>(vertices.size_bytes());
	glBufferData(GL_ARRAY_BUFFER, bytes, nullptr, GL_STREAM_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, vertices.data());

	glDrawArrays(primitive, 0, static_cast<GLsizei>(vertices.size()));
}

void applyScissor()
{
	if(clipStack.empty())
	{
		glDisable(GL_SCISSOR_TEST);
		return;
	}

	const glm::ivec4& box = clipStack.top();
	glScissor(box[0], box[1], box[2], box[3]);
	glEnable(GL_SCISSOR_TEST);
}

} // namespace

bool initDrawing(glm::ivec2 screenSize)
{
	if(!textureShader.build(Shaders::vertex, Shaders::fragmentTexture) ||
		!colorShader.build(Shaders::vertex, Shaders::fragmentColor))
	{
		deinitDrawing();
		return false;
	}

	// same projection as the former glOrtho(0, w, h, 0, -1, 1)
	const glm::mat4 projection =
		glm::ortho(0.0f, static_cast<float>(screenSize.x), static_cast<float>(screenSize.y), 0.0f, -1.0f, 1.0f);
	for(const Shader* shader : {&textureShader, &colorShader})
	{
		shader->use();
		shader->setProjection(projection);
	}
	modelView = glm::mat4(1.0f);

	// a VAO is mandatory in core: without one glDrawArrays draws nothing
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, pos)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, tex)));
	// normalized: bytes 0..255 become 0..1 in the shader
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(
		2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, color)));

	// every draw blends (the former code enabled it around each draw)
	glEnable(GL_BLEND);
	glActiveTexture(GL_TEXTURE0);

	clipStack = {};
	applyScissor();
	ready = true;
	return true;
}

bool isReady()
{
	return ready;
}

void deinitDrawing()
{
	ready = false;
	if(vbo != 0)
		glDeleteBuffers(1, &vbo);
	if(vao != 0)
		glDeleteVertexArrays(1, &vao);
	vbo = 0;
	vao = 0;
	textureShader.destroy();
	colorShader.destroy();
}

glm::ivec4 computeClipRect(
	const std::optional<glm::ivec4>& parent, glm::ivec2 pos, glm::ivec2 dim, glm::ivec2 screenSize)
{
	glm::ivec4 box(pos.x, pos.y, dim.x, dim.y);
	if(box[2] == 0)
		box[2] = screenSize.x - box.x;
	if(box[3] == 0)
		box[3] = screenSize.y - box.y;

	// glScissor starts at the bottom left of the window, so (0, 0, 1, 1) is the bottom left
	// pixel; everything else uses y+ = down, so flip it
	box[1] = screenSize.y - box.y - box[3];

	// make sure the box fits within the parent, and clip further accordingly
	if(parent)
	{
		const glm::ivec4& top = *parent;
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

	return box;
}

void pushClipRect(glm::ivec2 pos, glm::ivec2 dim)
{
	const std::optional<glm::ivec4> parent =
		clipStack.empty() ? std::nullopt : std::optional<glm::ivec4>(clipStack.top());
	clipStack.push(computeClipRect(
		parent, pos, dim, glm::ivec2(static_cast<int>(getScreenWidth()), static_cast<int>(getScreenHeight()))));
	applyScissor();
}

void popClipRect()
{
	if(clipStack.empty())
	{
		LOG(LogError) << "Tried to popClipRect while the stack was empty!";
		return;
	}

	clipStack.pop();
	applyScissor();
}

void setMatrix(const glm::mat4& transform)
{
	modelView = transform;
}

void drawRect(float x, float y, float w, float h, unsigned int color, BlendMode blend)
{
	drawRect((int)std::round(x), (int)std::round(y), (int)std::round(w), (int)std::round(h), color, blend);
}

void drawRect(int x, int y, int w, int h, unsigned int color, BlendMode blend)
{
	// the coordinates stay whole numbers (exact in float), as the former GL_INT array
	const float left = static_cast<float>(x);
	const float top = static_cast<float>(y);
	const float right = static_cast<float>(x + w);
	const float bottom = static_cast<float>(y + h);
	const std::uint32_t c = convertColor(color);

	const Vertex vertices[6] = {
		{{left, top}, {0.0f, 0.0f}, c},
		{{left, bottom}, {0.0f, 0.0f}, c},
		{{right, top}, {0.0f, 0.0f}, c},
		{{right, top}, {0.0f, 0.0f}, c},
		{{left, bottom}, {0.0f, 0.0f}, c},
		{{right, bottom}, {0.0f, 0.0f}, c},
	};
	draw(GL_TRIANGLES, vertices, 0, blend);
}

void drawTriangles(std::span<const Vertex> vertices, TextureHandle texture, BlendMode blend)
{
	draw(GL_TRIANGLES, vertices, texture, blend);
}

void drawLines(std::span<const Vertex> vertices, BlendMode blend)
{
	// width 1 (glLineWidth > 1 is deprecated in core)
	draw(GL_LINES, vertices, 0, blend);
}

} // namespace Renderer
