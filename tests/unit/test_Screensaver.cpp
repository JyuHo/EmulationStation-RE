// The window's sleep: a screensaver chosen by ScreenSaverBehavior starts after
// ScreenSaverTime without input and stops on the next input; the game clip one never
// plays the same clip twice in a row.
#include "GameClipScreensaver.h"
#include "Settings.h"
#include "Window.h"

#include <catch2/catch_test_macros.hpp>

namespace {

struct FakeScreensaver final : Screensaver
{
	explicit FakeScreensaver(bool canStart) : canStart(canStart) {}
	const char* behavior() const override { return "fake"; }
	bool start() override
	{
		started++;
		return canStart;
	}
	void stop() override { stopped++; }
	void update(int) override { updated++; }
	void render() override {}

	bool canStart;
	int started = 0;
	int stopped = 0;
	int updated = 0;
};

struct SleepSettings
{
	SleepSettings(const std::string& behavior)
	{
		Settings::getInstance()->setInt("ScreenSaverTime", 100);
		Settings::getInstance()->setString("ScreenSaverBehavior", behavior);
	}
	~SleepSettings()
	{
		Settings::getInstance()->setInt("ScreenSaverTime", 5 * 60 * 1000);
		Settings::getInstance()->setString("ScreenSaverBehavior", "dim");
	}
};

} // namespace

TEST_CASE("the screensaver named by ScreenSaverBehavior runs while the window sleeps", "[Screensaver]")
{
	SleepSettings settings("fake");
	Window window;
	auto owned = std::make_unique<FakeScreensaver>(true);
	FakeScreensaver* saver = owned.get();
	window.setScreensaver(std::move(owned));

	window.update(60);
	CHECK_FALSE(window.isSleeping());
	window.update(60);
	CHECK(window.isSleeping());
	CHECK(window.isScreensaverActive());
	CHECK(saver->started == 1);
	const int updates = saver->updated;
	window.update(16); // the screensaver, not the GUIs
	CHECK(saver->updated == updates + 1);

	// any input wakes the window (the input itself is dropped)
	window.input(nullptr, Input());
	CHECK_FALSE(window.isSleeping());
	CHECK_FALSE(window.isScreensaverActive());
	CHECK(saver->stopped == 1);
}

TEST_CASE("without anything to show, or another behavior, the window only dims", "[Screensaver]")
{
	SECTION("nothing to show")
	{
		SleepSettings settings("fake");
		Window window;
		window.setScreensaver(std::make_unique<FakeScreensaver>(false));
		window.update(150);
		CHECK(window.isSleeping());
		CHECK_FALSE(window.isScreensaverActive());
	}
	SECTION("dim")
	{
		SleepSettings settings("dim");
		Window window;
		auto owned = std::make_unique<FakeScreensaver>(true);
		FakeScreensaver* saver = owned.get();
		window.setScreensaver(std::move(owned));
		window.update(150);
		CHECK(window.isSleeping());
		CHECK(saver->started == 0);
	}
}

TEST_CASE("the game clips never repeat the previous one", "[Screensaver]")
{
	std::mt19937 random(42);
	CHECK(GameClipScreensaver::pick(1, 0, random) == 0);
	CHECK(GameClipScreensaver::pick(3, std::nullopt, random) < 3);

	std::size_t previous = 0;
	std::vector<int> seen(4, 0);
	for(int i = 0; i < 400; i++)
	{
		const std::size_t index = GameClipScreensaver::pick(4, previous, random);
		REQUIRE(index < 4);
		REQUIRE(index != previous);
		seen[index]++;
		previous = index;
	}
	for(int count : seen)
		CHECK(count > 50); // all of them come up
}
