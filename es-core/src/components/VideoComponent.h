#pragma once

#include "GuiComponent.h"
#include "components/ImageComponent.h"
#include "renderer/Renderer.h"
#include "video/SdlAudioSink.h"
#include "video/VideoPlayer.h"

#include <array>
#include <memory>
#include <string>

class ThemeData;

// Plays a video file in a loop, sized and placed like an ImageComponent (resize or maxSize,
// origin). Playback starts after a delay, so scrolling through a game list does not open
// every file; meanwhile, or when there is no video, a snapshot image can be shown. The frames
// are uploaded as three Luma8 textures and converted by the renderer's YUV shader
// (Renderer::drawYuv). While a video with sound plays, the background music is paused.
class VideoComponent : public GuiComponent
{
public:
	explicit VideoComponent(Window* window);
	~VideoComponent() override;

	// Plays path after the delay; an empty or missing path plays the default video. The same
	// path again keeps the current playback.
	void setVideo(const std::string& path);
	[[nodiscard]] const std::string& getVideo() const { return mPath; }
	// no video, not even the default one (a list scrolling, a view without video)
	void clearVideo();
	// Image shown during the delay and when there is no video, depending on the options below.
	void setImage(const std::string& path);
	void setDefaultVideo(const std::string& path) { mDefaultVideo = path; }
	void setDelay(int milliseconds) { mDelayMs = milliseconds; }
	void setShowSnapshotNoVideo(bool show) { mShowSnapshotNoVideo = show; }
	void setShowSnapshotDelay(bool show) { mShowSnapshotDelay = show; }
	// false: plays the video once, then hasEnded() (the game clip screensaver)
	void setLoop(bool loop) { mLoop = loop; }
	[[nodiscard]] bool hasEnded() const { return mEnded; }

	void setOrigin(float originX, float originY);
	void setOrigin(glm::vec2 origin) { setOrigin(origin.x, origin.y); }
	glm::vec2 getOrigin() const override { return mOrigin; }
	// like ImageComponent: a 0 component keeps the aspect ratio
	void setResize(float width, float height);
	void setResize(const glm::vec2& size) { setResize(size.x, size.y); }
	// fits inside the box, keeping the aspect ratio
	void setMaxSize(float width, float height);
	void setMaxSize(const glm::vec2& size) { setMaxSize(size.x, size.y); }
	[[nodiscard]] glm::vec2 getCenter() const;

	[[nodiscard]] bool isPlaying() const { return mPlayer && mPlayer->isOpen(); }
	[[nodiscard]] bool isWaiting() const { return mWaiting; }

	// stop the playback; it starts again after the delay once the component is shown
	void onHide() override;
	void onShow() override;

	void setOpacity(unsigned char opacity) override;
	void update(int deltaTime) override;
	void render(const glm::mat4& parentTrans) override;
	void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element,
		unsigned int properties) override;

	// Number of videos whose sound is playing (the background music is paused meanwhile).
	[[nodiscard]] static int audibleCount();

private:
	void start();
	void stop();
	void resize();
	void setMusicPaused(bool paused);
	void upload(const VideoPlayer::Frame& frame);
	void destroyTextures();
	[[nodiscard]] bool showSnapshot() const;

	std::string mPath;
	std::string mDefaultVideo;
	int mDelayMs = 2000;
	int mWaitLeftMs = 0;
	bool mWaiting = false; // a path waits for the delay
	bool mHidden = false;
	bool mShowSnapshotNoVideo = false;
	bool mShowSnapshotDelay = false;
	bool mPausingMusic = false;
	bool mLoop = true;
	bool mEnded = false;

	glm::vec2 mOrigin{0.0f};
	glm::vec2 mTargetSize{0.0f};
	bool mTargetIsMax = false;

	ImageComponent mSnapshot;

	std::unique_ptr<SdlAudioSink> mAudio; // before mPlayer: it outlives the player's threads
	std::unique_ptr<VideoPlayer> mPlayer;
	std::array<Renderer::TextureHandle, 3> mPlanes{};
	glm::ivec2 mFrameSize{0};
};
