#pragma once

#include "Screensaver.h"
#include "components/TextComponent.h"
#include "components/VideoComponent.h"

#include <cstddef>
#include <optional>
#include <random>
#include <string>
#include <vector>

class Window;

// "gameclip" screensaver (plan, phase 9; as in Recalbox): plays, full screen and one after
// the other, the videos of random games of the collection, with a banner naming the game
// and its system. Without any video, the window dims as before.
class GameClipScreensaver final : public Screensaver
{
public:
	struct Clip
	{
		std::string video;
		std::string game;
		std::string system;
	};

	explicit GameClipScreensaver(Window* window);

	[[nodiscard]] const char* behavior() const override { return "gameclip"; }
	bool start() override;
	void stop() override;
	void update(int deltaTime) override;
	void render() override;

	// The games of the loaded systems whose video file exists.
	static std::vector<Clip> collect();
	// A random index below count, other than previous when there is a choice.
	static std::size_t pick(std::size_t count, std::optional<std::size_t> previous, std::mt19937& random);

private:
	void next();

	std::vector<Clip> mClips;
	std::optional<std::size_t> mCurrent;
	std::mt19937 mRandom;
	VideoComponent mVideo;
	TextComponent mGame;
	TextComponent mSystem;
};
