#include "GameClipScreensaver.h"

#include "FileData.h"
#include "Log.h"
#include "SystemData.h"
#include "renderer/Renderer.h"
#include "resources/Font.h"

#include <filesystem>

namespace {

constexpr float kBannerTop = 0.84f; // of the screen height

} // namespace

GameClipScreensaver::GameClipScreensaver(Window* window) :
	mRandom(std::random_device{}()), mVideo(window), mGame(window), mSystem(window)
{
	mVideo.setDelay(0);
	mVideo.setLoop(false);
}

std::vector<GameClipScreensaver::Clip> GameClipScreensaver::collect()
{
	std::vector<Clip> clips;
	for(SystemData* system : SystemData::sSystemVector)
	{
		for(FileData* game : system->getRootFolder()->getFilesRecursive(GAME))
		{
			const std::string& video = game->metadata.get("video");
			std::error_code error;
			if(!video.empty() && std::filesystem::is_regular_file(video, error))
				clips.push_back({video, game->getName(), system->getFullName()});
		}
	}
	return clips;
}

std::size_t GameClipScreensaver::pick(std::size_t count, std::optional<std::size_t> previous, std::mt19937& random)
{
	if(count <= 1)
		return 0;
	// among the others: [0, count - 1) shifted past previous
	if(previous && *previous < count)
	{
		std::uniform_int_distribution<std::size_t> distribution(0, count - 2);
		const std::size_t index = distribution(random);
		return index >= *previous ? index + 1 : index;
	}
	std::uniform_int_distribution<std::size_t> distribution(0, count - 1);
	return distribution(random);
}

bool GameClipScreensaver::start()
{
	mClips = collect();
	mCurrent.reset();
	if(mClips.empty())
		return false;

	const float width = (float)Renderer::getScreenWidth();
	const float height = (float)Renderer::getScreenHeight();
	mVideo.setOrigin(0.5f, 0.5f);
	mVideo.setPosition(width / 2, height / 2);
	mVideo.setMaxSize(width, height);
	mVideo.onShow();

	mGame.setFont(Font::get(FONT_SIZE_MEDIUM));
	mGame.setColor(0xFFFFFFFF);
	mGame.setPosition(width * 0.03f, height * (kBannerTop + 0.02f));
	mSystem.setFont(Font::get(FONT_SIZE_SMALL));
	mSystem.setColor(0xC0C0C0FF);
	mSystem.setPosition(width * 0.03f, height * (kBannerTop + 0.08f));

	next();
	return true;
}

void GameClipScreensaver::stop()
{
	mVideo.clearVideo();
	mVideo.onHide();
	mClips.clear();
	mCurrent.reset();
}

void GameClipScreensaver::next()
{
	if(mClips.empty())
		return;
	mCurrent = pick(mClips.size(), mCurrent, mRandom);
	const Clip& clip = mClips[*mCurrent];
	mGame.setText(clip.game);
	mSystem.setText(clip.system);
	mVideo.setVideo(clip.video);
}

void GameClipScreensaver::update(int deltaTime)
{
	mVideo.update(deltaTime);
	if(mVideo.isPlaying() || mVideo.isWaiting() || !mCurrent)
		return;

	if(!mVideo.hasEnded())
	{
		// could not be opened (logged): not again
		mClips.erase(mClips.begin() + (std::ptrdiff_t)*mCurrent);
		mCurrent.reset();
	}
	next();
}

void GameClipScreensaver::render()
{
	const glm::mat4 identity(1.0f);
	const float width = (float)Renderer::getScreenWidth();
	const float height = (float)Renderer::getScreenHeight();

	Renderer::setMatrix(identity);
	Renderer::drawRect(0.0f, 0.0f, width, height, 0x000000FF);
	mVideo.render(identity);

	if(!mCurrent)
		return;
	Renderer::setMatrix(identity);
	Renderer::drawRect(0.0f, height * kBannerTop, width, height * (1.0f - kBannerTop), 0x000000A0);
	mGame.render(identity);
	mSystem.render(identity);
}
