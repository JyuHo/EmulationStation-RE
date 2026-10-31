#pragma once

// The renderer draws everything: rectangles, lines, textured triangles (images, fonts).
// This header is the module's public API and must stay free of any OpenGL type or header:
// only the files of es-core/src/renderer/ call gl* (through renderer/GL.h).
//   Renderer.cpp       global state, clip stack, matrices, draw calls
//   Batcher.cpp        merging of consecutive draws (no GL, tested)
//   Renderer_init.cpp  SDL3 window, OpenGL 3.3 core context, glad, GL_KHR_debug, swap
//   Shader.cpp         GLSL programs (sources in Shaders.h)
//   Texture.cpp        texture creation, update and deletion

#include "math/Math.h"

#include <bit>
#include <cstdint>
#include <optional>
#include <span>

struct SDL_Window;

namespace Renderer {

static_assert(std::endian::native == std::endian::little, "the vertex colours are packed for a little-endian CPU");

// 0xRRGGBBAA (the colour convention of the whole code base) -> bytes R, G, B, A in memory,
// which is what the colour attribute of the shaders reads (4 normalized unsigned bytes).
[[nodiscard]] constexpr std::uint32_t convertColor(unsigned int rgba)
{
	return ((rgba & 0xFF000000u) >> 24) | ((rgba & 0x00FF0000u) >> 8) | ((rgba & 0x0000FF00u) << 8) |
		((rgba & 0x000000FFu) << 24);
}

struct Vertex
{
	glm::vec2 pos;
	glm::vec2 tex;
	std::uint32_t color = 0; // convertColor(0xRRGGBBAA)
};
static_assert(sizeof(Vertex) == 20);

// (SRC_ALPHA, ONE_MINUS_SRC_ALPHA), (ONE, ONE), (ONE_MINUS_DST_COLOR, ZERO)
enum class BlendMode
{
	Alpha,
	Additive,
	InvertDst
};

// Alpha8: one channel, sampled as (1, 1, 1, a) (font atlases)
enum class TextureFormat
{
	RGBA8,
	Alpha8
};

enum class TextureFilter
{
	Nearest,
	LinearMinNearestMag
};

using TextureHandle = unsigned int; // 0 = no texture

bool init(int w, int h);
void deinit();

// logical size of the screen, used for the layout (the size at init)
unsigned int getScreenWidth();
unsigned int getScreenHeight();

// nullptr before init() and after deinit()
SDL_Window* getSDLWindow();

void swapBuffers();

void pushClipRect(glm::ivec2 pos, glm::ivec2 dim);
void popClipRect();

// Clip rectangle that pushClipRect(pos, dim) applies under the clip `parent` (the top of the
// stack, if any): {x, y, w, h} with y from the bottom of the screen (glScissor convention).
// A dimension of 0 extends the rectangle to the edge of the screen. Pure function (no GL).
[[nodiscard]] glm::ivec4 computeClipRect(
	const std::optional<glm::ivec4>& parent, glm::ivec2 pos, glm::ivec2 dim, glm::ivec2 screenSize);

// model-view matrix of the next draws (stays until the next call)
void setMatrix(const glm::mat4& transform);

void drawRect(float x, float y, float w, float h, unsigned int color, BlendMode blend = BlendMode::Alpha);
void drawRect(int x, int y, int w, int h, unsigned int color, BlendMode blend = BlendMode::Alpha);
void drawTriangles(std::span<const Vertex> vertices, TextureHandle texture, BlendMode blend = BlendMode::Alpha);
void drawLines(std::span<const Vertex> vertices, BlendMode blend = BlendMode::Alpha);

// Draws of the last complete frame: `requested` drawRect/drawTriangles/drawLines calls,
// `submitted` glDrawArrays (consecutive draws with the same texture, blending and primitive
// are merged into one, until the clip changes).
struct DrawCallCount
{
	unsigned int requested = 0;
	unsigned int submitted = 0;

	bool operator==(const DrawCallCount&) const = default;
};
DrawCallCount getDrawCallCount();

// data may be nullptr (empty atlas); rows are tightly packed (no row alignment)
[[nodiscard]] TextureHandle createTexture(
	TextureFormat format, TextureFilter filter, bool repeat, glm::ivec2 size, const void* data);
void updateTexture(TextureHandle texture, TextureFormat format, glm::ivec2 pos, glm::ivec2 size, const void* data);
void destroyTexture(TextureHandle texture);

} // namespace Renderer
