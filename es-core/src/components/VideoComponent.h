#pragma once

#include "GuiComponent.h"
#include "video/VideoPlayer.h"
#include "renderer/Renderer.h"

class VideoComponent : public GuiComponent
{
public:
	VideoComponent(Window* window);
	~VideoComponent();

	void setVideo(const std::string& path);
	void update(int deltaTime) override;
	void render(const glm::mat4& trans) override;
	void onSizeChanged() override;

private:
	VideoPlayer mPlayer;
	Renderer::TextureHandle mTexture = 0;
	std::vector<unsigned char> mPixels;
};