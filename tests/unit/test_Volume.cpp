// Sound settings: the sliders of the Sound menu with a d-pad, and the system volume helpers
// (percent <-> ALSA mixer range, choice of the mixer control).
#include "InputConfig.h"
#include "VolumeControl.h"
#include "components/SliderComponent.h"

#include <catch2/catch_test_macros.hpp>
#include <SDL3/SDL.h>

#include <string>
#include <vector>

namespace {

constexpr int kPad = 0;

// a pad whose d-pad is a hat, as SDL reports most of them
InputConfig hatPad()
{
	InputConfig config(kPad, "pad", "guid");
	config.mapInput("up", Input(kPad, TYPE_HAT, 0, SDL_HAT_UP, true));
	config.mapInput("down", Input(kPad, TYPE_HAT, 0, SDL_HAT_DOWN, true));
	config.mapInput("left", Input(kPad, TYPE_HAT, 0, SDL_HAT_LEFT, true));
	config.mapInput("right", Input(kPad, TYPE_HAT, 0, SDL_HAT_RIGHT, true));
	config.mapInput("a", Input(kPad, TYPE_BUTTON, 0, 1, true));
	return config;
}

Input hat(int value)
{
	return Input(kPad, TYPE_HAT, 0, value, false);
}

} // namespace

TEST_CASE("a slider only takes left/right presses, never the release of up/down", "[Volume][Slider]")
{
	InputConfig pad = hatPad();

	CHECK(classifySliderInput(&pad, hat(SDL_HAT_LEFT)) == SliderInput::Left);
	CHECK(classifySliderInput(&pad, hat(SDL_HAT_RIGHT)) == SliderInput::Right);
	CHECK(classifySliderInput(&pad, hat(SDL_HAT_RIGHTDOWN)) == SliderInput::Right);

	// the list moved onto the slider with "down": the hat's single release must reach the
	// list, or it keeps scrolling (the slider let it through only as Released)
	CHECK(classifySliderInput(&pad, hat(SDL_HAT_CENTERED)) == SliderInput::Released);
	// rolling from right to down stops the slider and lets the list move
	CHECK(classifySliderInput(&pad, hat(SDL_HAT_DOWN)) == SliderInput::Released);
	CHECK(classifySliderInput(&pad, hat(SDL_HAT_UP)) == SliderInput::Released);

	CHECK(classifySliderInput(&pad, Input(kPad, TYPE_BUTTON, 0, 1, false)) == SliderInput::Other);
	CHECK(classifySliderInput(&pad, Input(kPad, TYPE_HAT, 1, SDL_HAT_LEFT, false)) == SliderInput::Other);
}

TEST_CASE("a slider on keys or a stick: press and release of left/right", "[Volume][Slider]")
{
	InputConfig keyboard(DEVICE_KEYBOARD, "Keyboard", "-1");
	keyboard.mapInput("left", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_LEFT, 1, true));
	keyboard.mapInput("right", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_RIGHT, 1, true));
	keyboard.mapInput("down", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_DOWN, 1, true));

	CHECK(classifySliderInput(&keyboard, Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_LEFT, 1, false)) == SliderInput::Left);
	CHECK(
		classifySliderInput(&keyboard, Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_LEFT, 0, false)) == SliderInput::Released);
	CHECK(classifySliderInput(&keyboard, Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_DOWN, 0, false)) == SliderInput::Other);

	InputConfig stick(kPad, "stick", "guid");
	stick.mapInput("left", Input(kPad, TYPE_AXIS, 0, -1, true));
	stick.mapInput("right", Input(kPad, TYPE_AXIS, 0, 1, true));
	CHECK(classifySliderInput(&stick, Input(kPad, TYPE_AXIS, 0, -1, false)) == SliderInput::Left);
	CHECK(classifySliderInput(&stick, Input(kPad, TYPE_AXIS, 0, 1, false)) == SliderInput::Right);
	CHECK(classifySliderInput(&stick, Input(kPad, TYPE_AXIS, 0, 0, false)) == SliderInput::Released);
	CHECK(classifySliderInput(&stick, Input(kPad, TYPE_AXIS, 1, 1, false)) == SliderInput::Other);
}

TEST_CASE("the system volume converts to the mixer range and back without drifting", "[Volume]")
{
	// ranges of real controls: a PipeWire/PulseAudio "Master", an HDA "Master", a 0-31 codec
	const std::vector<std::pair<long, long>> ranges = {{0, 65536}, {0, 87}, {-10239, 400}, {0, 31}};
	for(const auto& [min, max] : ranges)
	{
		CHECK(volume::toRaw(0, min, max) == min);
		CHECK(volume::toRaw(100, min, max) == max);
		CHECK(volume::toPercent(min, min, max) == 0);
		CHECK(volume::toPercent(max, min, max) == 100);
		if(max - min < 100)
			continue; // fewer steps than percents: some percents share a step
		for(int percent = 0; percent <= 100; percent++)
		{
			INFO("range " << min << ".." << max << ", " << percent << "%");
			CHECK(volume::toPercent(volume::toRaw(percent, min, max), min, max) == percent);
		}
	}

	// 50% on 0-87 used to read back as 49%: each save of the menu lowered the volume
	CHECK(volume::toPercent(volume::toRaw(50, 0, 87), 0, 87) == 50);

	CHECK(volume::toRaw(-5, 0, 87) == 0);
	CHECK(volume::toRaw(150, 0, 87) == 87);
	CHECK(volume::toPercent(100, 0, 87) == 100);
	CHECK(volume::toPercent(5, 5, 5) == 0); // no range
}

TEST_CASE("the system volume drives the master control of the card", "[Volume]")
{
	using Names = std::vector<std::string>;
	// a Raspberry Pi or USB card has no "Master"
	CHECK(volume::chooseControl({"Capture", "Master", "PCM"}, "") == 1);
	CHECK(volume::chooseControl({"Mic", "PCM", "Headphone"}, "") == 1);
	CHECK(volume::chooseControl({"Mic", "Headphone", "Speaker"}, "") == 2);
	CHECK(volume::chooseControl({"Something"}, "") == 0);
	CHECK(volume::chooseControl(Names{}, "") == -1);

	// AudioDevice names one; gone, the automatic choice applies
	CHECK(volume::chooseControl({"Master", "PCM"}, "PCM") == 1);
	CHECK(volume::chooseControl({"Master", "PCM"}, "Headphone") == 0);
}
