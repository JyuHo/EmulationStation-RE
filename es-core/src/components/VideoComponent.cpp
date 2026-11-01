#include "components/VideoComponent.h"

#include "Log.h"
#include "MusicPlayer.h"
#include "Settings.h"
#include "ThemeData.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace {

int sAudible = 0;

bool fileExists(const std::string& path)
{
	std::error_code error;
	return !path.empty() && std::filesystem::is_regular_file(path, error);
}

} // namespace

VideoComponent::VideoComponent(Window* window) : GuiComponent(window), mSnapshot(window) {}

VideoComponent::~VideoComponent()
{
	stop(); // joins the player's threads before the sink goes
}

int VideoComponent::audibleCount()
{
	return sAudible;
}

void VideoComponent::setVideo(const std::string& path)
{
	if(path == mPath && (mWaiting || isPlaying()))
		return;
	stop();
	mEnded = false;
	mPath = path;
	mWaiting = fileExists(mPath) || fileExists(mDefaultVideo);
	mWaitLeftMs = mDelayMs;
}

void VideoComponent::clearVideo()
{
	stop();
	mPath.clear();
	mWaiting = false;
	mEnded = false;
}

void VideoComponent::setImage(const std::string& path)
{
	mSnapshot.setImage(path, false, true);
}

void VideoComponent::setOrigin(float originX, float originY)
{
	mOrigin = {originX, originY};
	mSnapshot.setOrigin(mOrigin);
}

void VideoComponent::setResize(float width, float height)
{
	mTargetSize = {width, height};
	mTargetIsMax = false;
	mSnapshot.setResize(mTargetSize);
	resize();
}

void VideoComponent::setMaxSize(float width, float height)
{
	mTargetSize = {width, height};
	mTargetIsMax = true;
	mSnapshot.setMaxSize(mTargetSize);
	resize();
}

glm::vec2 VideoComponent::getCenter() const
{
	return {mPosition.x - mSize.x * mOrigin.x + mSize.x / 2, mPosition.y - mSize.y * mOrigin.y + mSize.y / 2};
}

void VideoComponent::onHide()
{
	mHidden = true;
	if(isPlaying())
	{
		// played again from the start, after the delay, when shown
		stop();
		mWaiting = true;
	}
	mWaitLeftMs = mDelayMs;
	GuiComponent::onHide();
}

void VideoComponent::onShow()
{
	mHidden = false;
	GuiComponent::onShow();
}

void VideoComponent::setOpacity(unsigned char opacity)
{
	GuiComponent::setOpacity(opacity);
	mSnapshot.setOpacity(opacity);
}

void VideoComponent::start()
{
	mWaiting = false;
	const std::string& path = fileExists(mPath) ? mPath : mDefaultVideo;
	if(!fileExists(path))
		return;

	if(Settings::getInstance()->getBool("VideoAudio"))
	{
		mAudio = std::make_unique<SdlAudioSink>();
		if(!mAudio->isValid())
			mAudio.reset();
	}
	VideoAudioSink* sink = mAudio.get();
	VideoPlayer::Options options;
	options.audio = sink;
	mPlayer = std::make_unique<VideoPlayer>(std::move(options));
	if(auto opened = mPlayer->open(path); !opened)
	{
		Log::write(LogError, "Could not open video {}: {}", path, opened.error());
		mPlayer.reset();
		return;
	}
	setMusicPaused(mPlayer->hasAudio() && sink != nullptr);
}

void VideoComponent::stop()
{
	mPlayer.reset();
	// a sink per playback: the audio device is closed and opened again around a game
	mAudio.reset();
	setMusicPaused(false);
	destroyTextures();
}

void VideoComponent::setMusicPaused(bool paused)
{
	if(paused == mPausingMusic)
		return;
	mPausingMusic = paused;
	if(paused && sAudible++ == 0)
		MusicPlayer::getInstance().pause();
	else if(!paused && --sAudible == 0)
		MusicPlayer::getInstance().resume();
}

bool VideoComponent::showSnapshot() const
{
	if(isPlaying() && mPlanes[0] != 0)
		return false;
	if(mWaiting || isPlaying())
		return mShowSnapshotDelay;
	return mShowSnapshotNoVideo;
}

void VideoComponent::resize()
{
	if(!isPlaying() || mPlayer->width() <= 0 || mPlayer->height() <= 0)
	{
		// the snapshot, or the box
		mSize = showSnapshot() && mSnapshot.hasImage() ? mSnapshot.getSize() : mTargetSize;
		return;
	}

	const glm::vec2 source((float)mPlayer->width(), (float)mPlayer->height());
	glm::vec2 size = mTargetSize;
	if(mTargetIsMax)
	{
		const float scale = std::min(mTargetSize.x / source.x, mTargetSize.y / source.y);
		size = source * scale;
	}
	else if(mTargetSize.x == 0 && mTargetSize.y == 0)
		size = source;
	else if(mTargetSize.x == 0)
		size.x = mTargetSize.y * source.x / source.y;
	else if(mTargetSize.y == 0)
		size.y = mTargetSize.x * source.y / source.x;
	mSize = glm::round(size);
}

void VideoComponent::update(int deltaTime)
{
	if(mWaiting && !mHidden)
	{
		mWaitLeftMs -= deltaTime;
		if(mWaitLeftMs <= 0)
			start();
	}

	// loop: the whole file again once every frame was shown
	if(mPlayer && mPlayer->finished())
	{
		if(!mLoop)
		{
			stop();
			mEnded = true;
		}
		else if(auto opened = mPlayer->open(fileExists(mPath) ? mPath : mDefaultVideo); !opened)
			stop();
	}

	mSnapshot.update(deltaTime);
	resize();
	GuiComponent::update(deltaTime);
}

void VideoComponent::destroyTextures()
{
	for(Renderer::TextureHandle& plane : mPlanes)
	{
		Renderer::destroyTexture(plane);
		plane = 0;
	}
	mFrameSize = glm::ivec2(0);
}

void VideoComponent::upload(const VideoPlayer::Frame& frame)
{
	const glm::ivec2 size(frame.width, frame.height);
	const glm::ivec2 chroma((frame.width + 1) / 2, (frame.height + 1) / 2);
	const std::array<const std::vector<std::uint8_t>*, 3> data{&frame.y, &frame.u, &frame.v};
	if(size != mFrameSize || mPlanes[0] == 0)
	{
		destroyTextures();
		for(std::size_t i = 0; i < 3; i++)
			mPlanes[i] = Renderer::createTexture(Renderer::TextureFormat::Luma8,
				Renderer::TextureFilter::LinearMinNearestMag, false, i == 0 ? size : chroma, data[i]->data());
		mFrameSize = size;
		return;
	}
	for(std::size_t i = 0; i < 3; i++)
		Renderer::updateTexture(
			mPlanes[i], Renderer::TextureFormat::Luma8, glm::ivec2(0), i == 0 ? size : chroma, data[i]->data());
}

void VideoComponent::render(const glm::mat4& parentTrans)
{
	const glm::mat4 trans = roundMatrix(parentTrans * getTransform());

	if(mPlayer)
		if(const VideoPlayer::Frame* frame = mPlayer->currentFrame())
			upload(*frame);

	if(isPlaying() && mPlanes[0] != 0)
	{
		if(mOpacity > 0)
		{
			Renderer::setMatrix(trans);
			const std::uint32_t color = Renderer::convertColor(0xFFFFFF00 | mOpacity);
			const float x0 = std::floor(-mSize.x * mOrigin.x);
			const float y0 = std::floor(-mSize.y * mOrigin.y);
			const float x1 = x0 + std::round(mSize.x);
			const float y1 = y0 + std::round(mSize.y);
			// frame rows are top-down: texture row 0 is the top of the picture
			const Renderer::Vertex vertices[6] = {
				{{x0, y0}, {0, 0}, color},
				{{x0, y1}, {0, 1}, color},
				{{x1, y0}, {1, 0}, color},
				{{x1, y0}, {1, 0}, color},
				{{x0, y1}, {0, 1}, color},
				{{x1, y1}, {1, 1}, color},
			};
			Renderer::drawYuv(vertices, mPlanes);
		}
	}
	else if(showSnapshot())
		mSnapshot.render(trans);

	GuiComponent::renderChildren(trans);
}

void VideoComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view,
	const std::string& element, unsigned int properties)
{
	using namespace ThemeFlags;

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "video");
	if(!elem)
		return;

	const glm::vec2 scale = getParent()
		? getParent()->getSize()
		: glm::vec2((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	if(properties & POSITION && elem->has("pos"))
	{
		const glm::vec2 denormalized = elem->get<glm::vec2>("pos") * scale;
		setPosition(glm::vec3(denormalized.x, denormalized.y, 0));
	}

	if(properties & ThemeFlags::SIZE)
	{
		if(elem->has("size"))
			setResize(elem->get<glm::vec2>("size") * scale);
		else if(elem->has("maxSize"))
			setMaxSize(elem->get<glm::vec2>("maxSize") * scale);
	}

	// position + size also implies origin
	if((properties & ORIGIN || (properties & POSITION && properties & ThemeFlags::SIZE)) && elem->has("origin"))
		setOrigin(elem->get<glm::vec2>("origin"));

	if(properties & PATH && elem->has("default"))
		setDefaultVideo(elem->get<std::string>("default"));

	if(elem->has("delay"))
		setDelay((int)std::lround(elem->get<float>("delay") * 1000.0f));
	if(elem->has("showSnapshotNoVideo"))
		setShowSnapshotNoVideo(elem->get<bool>("showSnapshotNoVideo"));
	if(elem->has("showSnapshotDelay"))
		setShowSnapshotDelay(elem->get<bool>("showSnapshotDelay"));

	applyCommonTheme(theme, view, element, properties);
}
