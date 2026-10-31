#pragma once

#include "GuiComponent.h"
#include "renderer/Renderer.h"
#include "video/SdlAudioSink.h"
#include "video/VideoPlayer.h"

#include <array>
#include <memory>
#include <string>

// Plays a video file in its rectangle, in a loop. The frames are uploaded as three Luma8
// textures and converted by the renderer's YUV shader (Renderer::drawYuv).
class VideoComponent : public GuiComponent
{
public:
	explicit VideoComponent(Window* window);
	~VideoComponent() override;

	// empty path: stops
	void setVideo(const std::string& path);
	[[nodiscard]] const std::string& getVideo() const { return mPath; }
	[[nodiscard]] bool isPlaying() const { return mPlayer && mPlayer->isOpen(); }

	void pause();
	void resume();

	void update(int deltaTime) override;
	void render(const glm::mat4& parentTrans) override;

private:
	void upload(const VideoPlayer::Frame& frame);
	void destroyTextures();

	std::string mPath;
	std::unique_ptr<SdlAudioSink> mAudio; // before mPlayer: it outlives the player's threads
	std::unique_ptr<VideoPlayer> mPlayer;
	std::array<Renderer::TextureHandle, 3> mPlanes{};
	glm::ivec2 mFrameSize{0};
};
