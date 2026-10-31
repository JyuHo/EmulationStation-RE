#include "renderer/Batcher.h"

#include <utility>

namespace Renderer {

Batcher::Batcher(Submit submit) : mSubmit(std::move(submit)) {}

void Batcher::add(const BatchState& state, std::span<const Vertex> vertices, const glm::mat4& transform)
{
	if(vertices.empty())
		return;

	if(!mVertices.empty() && !(state == mState))
		flush();
	mState = state;

	mVertices.reserve(mVertices.size() + vertices.size());
	for(const Vertex& vertex : vertices)
	{
		Vertex transformed = vertex;
		const glm::vec3 pos = transformPoint(transform, glm::vec3(vertex.pos, 0.0f));
		transformed.pos = glm::vec2(pos.x, pos.y);
		mVertices.push_back(transformed);
	}
}

void Batcher::flush()
{
	if(mVertices.empty())
		return;

	mSubmit(mState, mVertices);
	mVertices.clear();
}

void Batcher::clear()
{
	mVertices.clear();
}

} // namespace Renderer
