#pragma once

#include "ThemeData.h"
#include "math/Math.h"
#include "renderer/Renderer.h"

#include <string>

namespace es::theme {

// Menu theme: values from the "menu" view of the current theme, with ES defaults.
struct MenuTheme
{
	// background
	unsigned int backgroundColor = 0x000000FF;
	std::string backgroundPath;

	// title text
	unsigned int titleColor = 0xFFFFFFFF;
	std::string titleFontPath;
	float titleFontSize = 0.045f;

	// entry text
	unsigned int entryColor = 0xFFFFFFFF;
	unsigned int selectedColor = 0xFFFFFFFF;
	unsigned int separatorColor = 0x777777FF;

	// selection
	unsigned int selectorColor = 0x777777FF;

	// small text
	unsigned int smallColor = 0xFFFFFFFF;
	float smallFontSize = 0.035f;

	// switches
	std::string switchOnPath;
	std::string switchOffPath;

	// slider
	std::string sliderPath;

	// button
	std::string buttonPath;
	std::string buttonFilledPath;

	// help
	unsigned int helpTextColor = 0x777777FF;
	unsigned int helpIconColor = 0x777777FF;
	std::string helpFontPath;
	float helpFontSize = 0.035f;

	static MenuTheme fromTheme(const std::shared_ptr<ThemeData>& theme);
};

} // namespace es::theme
