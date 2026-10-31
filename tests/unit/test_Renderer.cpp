// Logic of the renderer that needs no OpenGL context (ctest runs without a display): the
// colour packing of the vertices, the clip rectangles given to glScissor and the batching of
// consecutive draws (with a fake submission in place of glDrawArrays).
#include "renderer/Batcher.h"
#include "renderer/Renderer.h"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstring>
#include <optional>
#include <vector>

namespace {

std::array<unsigned char, 4> bytesOf(std::uint32_t value)
{
	std::array<unsigned char, 4> bytes{};
	std::memcpy(bytes.data(), &value, sizeof(value));
	return bytes;
}

const glm::ivec2 screen(1280, 720);

using Renderer::BatchState;
using Renderer::BlendMode;
using Renderer::Primitive;
using Renderer::Vertex;

struct Submission
{
	BatchState state;
	std::vector<Vertex> vertices;
};

// a Batcher whose submissions are recorded
struct FakeRenderer
{
	std::vector<Submission> submissions;
	Renderer::Batcher batcher{[this](const BatchState& state, std::span<const Vertex> vertices) {
		submissions.push_back({state, std::vector<Vertex>(vertices.begin(), vertices.end())});
	}};
};

// a quad (2 triangles) whose first vertex is at (x, 0), tagged by its colour
std::vector<Vertex> quad(float x, std::uint32_t tag)
{
	std::vector<Vertex> v(6);
	for(std::size_t i = 0; i < v.size(); i++)
		v[i] = {{x + (float)i, 0.0f}, {0.0f, 0.0f}, tag};
	return v;
}

const glm::mat4 identity(1.0f);
const BatchState imageA{Primitive::Triangles, 1, BlendMode::Alpha};

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

TEST_CASE("Batcher merges consecutive draws with the same state", "[Renderer]")
{
	FakeRenderer r;
	r.batcher.add(imageA, quad(0, 1), identity);
	r.batcher.add(imageA, quad(10, 2), identity);
	CHECK(r.submissions.empty());
	CHECK(r.batcher.pendingVertices() == 12);

	r.batcher.flush();
	REQUIRE(r.submissions.size() == 1);
	CHECK(r.submissions[0].state == imageA);
	REQUIRE(r.submissions[0].vertices.size() == 12);
	// order kept inside the batch
	CHECK(r.submissions[0].vertices[0].color == 1);
	CHECK(r.submissions[0].vertices[6].color == 2);
	CHECK(r.batcher.pendingVertices() == 0);
}

TEST_CASE("Batcher flushes on every state change", "[Renderer]")
{
	const BatchState otherTexture{Primitive::Triangles, 2, BlendMode::Alpha};
	const BatchState otherBlend{Primitive::Triangles, 2, BlendMode::Additive};
	const BatchState lines{Primitive::Lines, 0, BlendMode::Additive};
	const BatchState colour{Primitive::Triangles, 0, BlendMode::Additive};

	FakeRenderer r;
	r.batcher.add(imageA, quad(0, 1), identity);
	r.batcher.add(otherTexture, quad(0, 2), identity);
	r.batcher.add(otherBlend, quad(0, 3), identity);
	r.batcher.add(lines, quad(0, 4), identity);
	r.batcher.add(colour, quad(0, 5), identity);
	r.batcher.flush();

	REQUIRE(r.submissions.size() == 5);
	CHECK(r.submissions[0].state == imageA);
	CHECK(r.submissions[1].state == otherTexture);
	CHECK(r.submissions[2].state == otherBlend);
	CHECK(r.submissions[3].state == lines);
	CHECK(r.submissions[4].state == colour);
	for(std::size_t i = 0; i < r.submissions.size(); i++)
		CHECK(r.submissions[i].vertices[0].color == i + 1);
}

TEST_CASE("Batcher never reorders: A B A gives three submissions", "[Renderer]")
{
	const BatchState imageB{Primitive::Triangles, 2, BlendMode::Alpha};

	FakeRenderer r;
	r.batcher.add(imageA, quad(0, 1), identity);
	r.batcher.add(imageB, quad(0, 2), identity);
	r.batcher.add(imageA, quad(0, 3), identity);
	r.batcher.flush();

	REQUIRE(r.submissions.size() == 3);
	CHECK(r.submissions[0].vertices[0].color == 1);
	CHECK(r.submissions[1].vertices[0].color == 2);
	CHECK(r.submissions[2].vertices[0].color == 3);
}

TEST_CASE("Batcher: an explicit flush (clip change) splits identical draws", "[Renderer]")
{
	FakeRenderer r;
	r.batcher.add(imageA, quad(0, 1), identity);
	r.batcher.flush(); // what Renderer does in pushClipRect/popClipRect
	r.batcher.add(imageA, quad(0, 2), identity);
	r.batcher.flush();

	REQUIRE(r.submissions.size() == 2);
	CHECK(r.submissions[0].vertices.size() == 6);
	CHECK(r.submissions[1].vertices.size() == 6);
}

TEST_CASE("Batcher: nothing pending, nothing submitted", "[Renderer]")
{
	FakeRenderer r;
	r.batcher.flush();
	r.batcher.add(imageA, {}, identity);
	r.batcher.flush();
	CHECK(r.submissions.empty());

	r.batcher.add(imageA, quad(0, 1), identity);
	r.batcher.clear();
	r.batcher.flush();
	CHECK(r.submissions.empty());
}

TEST_CASE("Batcher transforms the vertices on the CPU", "[Renderer]")
{
	const glm::mat4 moved = glm::scale(glm::translate(identity, glm::vec3(100, 50, 0)), glm::vec3(2, 3, 1));

	FakeRenderer r;
	r.batcher.add(imageA, quad(1, 1), moved);
	r.batcher.add(imageA, quad(1, 2), identity); // other matrix, same batch
	r.batcher.flush();

	REQUIRE(r.submissions.size() == 1);
	CHECK(r.submissions[0].vertices[0].pos == glm::vec2(102, 50));
	CHECK(r.submissions[0].vertices[1].pos == glm::vec2(104, 50));
	CHECK(r.submissions[0].vertices[6].pos == glm::vec2(1, 0));
	// texture coordinates and colour untouched
	CHECK(r.submissions[0].vertices[0].tex == glm::vec2(0, 0));
	CHECK(r.submissions[0].vertices[0].color == 1);
}
