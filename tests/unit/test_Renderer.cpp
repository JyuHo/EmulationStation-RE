// Logic of the renderer that needs no OpenGL context (ctest runs without a display): the
// colour packing of the vertices and the clip rectangles given to glScissor.
#include "renderer/Renderer.h"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstring>
#include <optional>

namespace {

std::array<unsigned char, 4> bytesOf(std::uint32_t value)
{
	std::array<unsigned char, 4> bytes{};
	std::memcpy(bytes.data(), &value, sizeof(value));
	return bytes;
}

const glm::ivec2 screen(1280, 720);

} // namespace

TEST_CASE("convertColor puts R, G, B, A in memory order", "[Renderer]")
{
	CHECK(bytesOf(Renderer::convertColor(0x11223344)) == std::array<unsigned char, 4>{0x11, 0x22, 0x33, 0x44});
	CHECK(bytesOf(Renderer::convertColor(0xFFFFFF80)) == std::array<unsigned char, 4>{0xFF, 0xFF, 0xFF, 0x80});
	CHECK(bytesOf(Renderer::convertColor(0x000000FF)) == std::array<unsigned char, 4>{0x00, 0x00, 0x00, 0xFF});

	// usable in constant expressions
	static_assert(Renderer::convertColor(0x11223344) == 0x44332211u);
}

TEST_CASE("Renderer::Vertex has the layout of the vertex attributes", "[Renderer]")
{
	STATIC_CHECK(sizeof(Renderer::Vertex) == 20);
	STATIC_CHECK(offsetof(Renderer::Vertex, pos) == 0);
	STATIC_CHECK(offsetof(Renderer::Vertex, tex) == 8);
	STATIC_CHECK(offsetof(Renderer::Vertex, color) == 16);
}

TEST_CASE("computeClipRect flips y to the bottom-left origin of glScissor", "[Renderer]")
{
	CHECK(Renderer::computeClipRect(std::nullopt, {10, 20}, {100, 50}, screen) == glm::ivec4(10, 650, 100, 50));
}

TEST_CASE("computeClipRect: a dimension of 0 goes to the edge of the screen", "[Renderer]")
{
	CHECK(Renderer::computeClipRect(std::nullopt, {10, 20}, {0, 0}, screen) == glm::ivec4(10, 0, 1270, 700));
	CHECK(Renderer::computeClipRect(std::nullopt, {10, 20}, {0, 50}, screen) == glm::ivec4(10, 650, 1270, 50));
}

TEST_CASE("computeClipRect: nested rectangle inside its parent is unchanged", "[Renderer]")
{
	const glm::ivec4 parent = Renderer::computeClipRect(std::nullopt, {100, 100}, {400, 300}, screen);
	REQUIRE(parent == glm::ivec4(100, 320, 400, 300));

	CHECK(Renderer::computeClipRect(parent, {150, 150}, {100, 100}, screen) == glm::ivec4(150, 470, 100, 100));
}

TEST_CASE("computeClipRect: rectangle overflowing its parent", "[Renderer]")
{
	const glm::ivec4 parent(0, 0, 1280, 720);

	// cut on the right; on the bottom (y < 0 in scissor coordinates) the origin moves to the
	// parent's edge but the height is kept: behaviour of the code since the original
	// EmulationStation, characterized here
	CHECK(Renderer::computeClipRect(parent, {1200, 700}, {200, 100}, screen) == glm::ivec4(1200, 0, 80, 100));

	// cut on the left and the top
	CHECK(Renderer::computeClipRect(parent, {1200, -50}, {200, 100}, screen) == glm::ivec4(1200, 670, 80, 50));
}

TEST_CASE("computeClipRect: disjoint rectangles give an empty clip", "[Renderer]")
{
	const glm::ivec4 parent = Renderer::computeClipRect(std::nullopt, {0, 620}, {100, 100}, screen);
	REQUIRE(parent == glm::ivec4(0, 0, 100, 100));

	const glm::ivec4 box = Renderer::computeClipRect(parent, {200, 200}, {10, 10}, screen);
	CHECK(box[2] == 0);
	CHECK(box[3] == 0);
}
