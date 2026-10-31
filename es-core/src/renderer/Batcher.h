#pragma once

#include "renderer/Renderer.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace Renderer {

enum class Primitive
{
	Triangles,
	Lines
};

// What must not change inside a batch. The program follows from the texture (0 = colour
// program); the clip is not part of it: Renderer flushes before every clip change.
struct BatchState
{
	Primitive primitive = Primitive::Triangles;
	TextureHandle texture = 0;
	BlendMode blend = BlendMode::Alpha;

	bool operator==(const BatchState&) const = default;
};

// Merges consecutive draws that share a BatchState into one submission, without ever
// reordering them (transparency depends on the order). Vertices are transformed by the
// model-view matrix on the CPU when added, so a batch can mix matrices. No GL: the
// submission is a callback (glDrawArrays in Renderer.cpp, a fake in the tests).
class Batcher
{
public:
	using Submit = std::function<void(const BatchState&, std::span<const Vertex>)>;

	explicit Batcher(Submit submit);

	void add(const BatchState& state, std::span<const Vertex> vertices, const glm::mat4& transform);

	// submits the pending vertices, if any (no call when there are none)
	void flush();

	// drops the pending vertices without submitting them (the GL context is gone)
	void clear();

	std::size_t pendingVertices() const { return mVertices.size(); }

private:
	Submit mSubmit;
	BatchState mState;
	std::vector<Vertex> mVertices;
};

} // namespace Renderer
