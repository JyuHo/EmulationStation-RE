#include "components/VideoComponent.h"

#include "Log.h"
#include "renderer/Renderer.h"

#include <span>

VideoComponent::VideoComponent(Window* window) : GuiComponent(window) {}

VideoComponent::~VideoComponent()
{
	if(mTexture != 0)
		Renderer::destroyTexture(mTexture);
}

void VideoComponent::setVideo(const std::string& path)
{
	if(mTexture != 0)
	{
		Renderer::destroyTexture(mTexture);
		mTexture = 0;
	}
	mPixels.clear();
	if(!mPlayer.open(path))
	{
		LOG(LogError) << "Could not open video " << path;
		return;
	}
	mPixels.assign((size_t)mPlayer.width() * mPlayer.height() * 4, 0);
	mTexture = Renderer::createTexture(Renderer::TextureFormat::RGBA8, Renderer::TextureFilter::LinearMinNearestMag,
		false, glm::ivec2(mPlayer.width(), mPlayer.height()), nullptr);
}

void VideoComponent::update(int deltaTime)
{
	if(!mPlayer.isOpen()) return;
	if(mPlayer.update(deltaTime))
	{
		const auto& f = mPlayer.frame();
		if(!f.empty() && mTexture != 0)
			Renderer::updateTexture(mTexture, Renderer::TextureFormat::RGBA8, glm::ivec2(0, 0),
				glm::ivec2(mPlayer.width(), mPlayer.height()), f.data());
	}
	GuiComponent::update(deltaTime);
}

void VideoComponent::render(const glm::mat4& trans)
{
	if(mTexture == 0) return;
	glm::mat4 t = getTransform() * trans;
	Renderer::setMatrix(t);

	Renderer::Vertex verts[6];
	float w = (float)mSize.x;
	float h = (float)mSize.y;
	verts[0] = { {0, 0}, {0, 1}, 0xFFFFFFFF };
	verts[1] = { {0, h}, {0, 0}, 0xFFFFFFFF };
	verts[2] = { {w, 0}, {1, 1}, 0xFFFFFFFF };
	verts[3] = { {w, 0}, {1, 1}, 0xFFFFFFFF };
	verts[4] = { {0, h}, {0, 0}, 0xFFFFFFFF };
	verts[5] = { {w, h}, {1, 0}, 0xFFFFFFFF };
	Renderer::drawTriangles(std::span<const Renderer::Vertex>(verts, 6), mTexture);
}

void VideoComponent::onSizeChanged() {}