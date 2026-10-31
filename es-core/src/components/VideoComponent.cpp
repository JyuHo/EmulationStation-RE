#include "components/VideoComponent.h"

#include "Log.h"
#include "Settings.h"

VideoComponent::VideoComponent(Window* window) : GuiComponent(window) {}

VideoComponent::~VideoComponent()
{
	mPlayer.reset(); // joins its threads before the sink goes
	destroyTextures();
}

void VideoComponent::setVideo(const std::string& path)
{
	if(path == mPath && isPlaying())
		return;
	mPlayer.reset();
	mPath = path;
	if(path.empty())
		return;

	if(Settings::getInstance()->getBool("VideoAudio") && !mAudio)
	{
		mAudio = std::make_unique<SdlAudioSink>();
		if(!mAudio->isValid())
			mAudio.reset();
	}
	VideoPlayer::Options options;
	options.audio = mAudio.get();
	mPlayer = std::make_unique<VideoPlayer>(std::move(options));
	if(auto opened = mPlayer->open(path); !opened)
	{
		Log::write(LogError, "Could not open video {}: {}", path, opened.error());
		mPlayer.reset();
	}
}

void VideoComponent::pause()
{
	if(mPlayer)
		mPlayer->pause();
}

void VideoComponent::resume()
{
	if(mPlayer)
		mPlayer->resume();
}

void VideoComponent::update(int deltaTime)
{
	// loop: the whole file again once every frame was shown
	if(mPlayer && mPlayer->finished())
	{
		if(auto opened = mPlayer->open(mPath); !opened)
			mPlayer.reset();
	}
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

	if(mPlanes[0] != 0 && mOpacity > 0)
	{
		Renderer::setMatrix(trans);
		const std::uint32_t color = Renderer::convertColor(0xFFFFFF00 | mOpacity);
		const float w = std::round(mSize.x);
		const float h = std::round(mSize.y);
		// frame rows are top-down: texture row 0 is the top of the picture
		const Renderer::Vertex vertices[6] = {
			{{0, 0}, {0, 0}, color},
			{{0, h}, {0, 1}, color},
			{{w, 0}, {1, 0}, color},
			{{w, 0}, {1, 0}, color},
			{{0, h}, {0, 1}, color},
			{{w, h}, {1, 1}, color},
		};
		Renderer::drawYuv(vertices, mPlanes);
	}

	GuiComponent::renderChildren(trans);
}
