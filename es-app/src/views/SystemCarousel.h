#pragma once

#include "ThemeData.h"

#include <glm/glm.hpp>
#include <optional>
#include <string>

// Themed system carousel (docs/THEMES.md, element `carousel`, PR 11g): the values of the
// element and the placement of the logos, without rendering, so that they can be tested.
// Model: Recalbox SystemView (views/SystemView.cpp @10.1.1), reference only.
namespace es::carousel {

enum class Type
{
	Horizontal,
	Vertical,
	VerticalWheel
};

enum class HAlign
{
	Left,
	Center,
	Right
};

enum class VAlign
{
	Top,
	Center,
	Bottom
};

struct Carousel
{
	Type type = Type::Horizontal;
	glm::vec2 pos{0.0f};  // pixels
	glm::vec2 size{0.0f}; // pixels
	glm::vec2 origin{0.0f};
	unsigned int color = 0xFFFFFFD8;
	float logoScale = 1.2f;
	float logoRotation = 7.5f;
	glm::vec2 logoRotationOrigin{-5.0f, 0.5f};
	glm::vec2 logoSize{0.0f}; // pixels
	HAlign hAlign = HAlign::Center;
	VAlign vAlign = VAlign::Center;
	int maxLogoCount = 3;
	std::optional<std::string> fontPath;
	std::optional<float> fontSize; // fraction of the screen height
	unsigned int fontColor = 0xE6E6E6FF;
	// text list (textOnly)
	bool textOnly = false;
	bool forceUppercase = false;
	unsigned int selectorColor = 0xFFFFFF33;
	unsigned int selectedColor = 0xFFFFFFFF;
	unsigned int primaryColor = 0xE6E6E6FF;
	unsigned int secondaryColor = 0xC6C6C6FF;
	float selectorHeight = 0.0f; // pixels, 0: the row height
	float selectorOffsetX = 0.0f;
	float selectorOffsetY = 0.0f;
	float textOffsetX = 0.0f;
	float lineSpacing = 1.5f;
	float horizontalMargin = 0.0f;
	float zIndex = 40.0f;
	std::optional<std::string> defaultTransition;
};

// Values without a theme element (Recalbox SystemView::getDefaultElements, SystemView.cpp:982-1015)
// for a screen of the given size.
[[nodiscard]] Carousel defaults(glm::vec2 screen);

// defaults(screen), then each property of the element (getCarouselFromTheme,
// SystemView.cpp:1029-1071). nullptr: defaults(screen).
[[nodiscard]] Carousel fromElement(const ThemeData::ThemeElement* elem, glm::vec2 screen);

// "vertical", "vertical_wheel", anything else: horizontal (SystemView.cpp:1033-1037)
[[nodiscard]] Type parseType(const std::string& value);
// Recalbox alignment names (themes/ThemeElement.cpp:68-90); unknown: center left
void parseAlignment(const std::string& value, HAlign& h, VAlign& v);

// Logo `i` of the list (unwrapped index, may be negative or past the end): top left corner of
// its slot, in the carousel's coordinates, for a camera at `camOffset` (list index units).
// The slot has the size logoSize (SystemView.cpp:651-697).
[[nodiscard]] glm::vec2 slotPosition(const Carousel& c, float camOffset, int i);
// Point of the slot the logo refers to (its origin, chosen by the alignment, AlignComponent,
// SystemView.cpp:41-57 and 150-163), relative to the slot.
[[nodiscard]] glm::vec2 logoOrigin(const Carousel& c);
// Scale of a logo at `distance` (i - camOffset) from the camera, relative to its full size
// logoSize x logoScale: 1 for the selected logo, 1 / logoScale from one slot on.
[[nodiscard]] float logoScaleFactor(const Carousel& c, float distance);
// 0x80 from one slot on, 0xFF for the selected logo
[[nodiscard]] unsigned char logoOpacity(float distance);
// logoRotation x distance on a wheel, 0 otherwise
[[nodiscard]] float logoRotationAt(const Carousel& c, float distance);
// First and last unwrapped indexes to draw: maxLogoCount (at most the list size) around the
// camera, plus one on each side while there are several logos
[[nodiscard]] glm::ivec2 visibleRange(const Carousel& c, float camOffset, int entryCount);
// index modulo count, in [0, count)
[[nodiscard]] int wrapIndex(int index, int count);

// Text list (textOnly, SystemView.cpp:789-885)
struct TextListLayout
{
	float rowHeight = 0.0f;
	float startOffset = 0.0f; // index of the row at the top (fraction)
	int firstRow = 0;
	int rowCount = 0;		  // rows to draw from firstRow
	float selectorY = 0.0f;
	float selectorHeight = 0.0f;
};
[[nodiscard]] TextListLayout textListLayout(const Carousel& c, float fontHeight, int cursor, int entryCount);
// text color of row `index`: selected, else secondary for odd rows, primary for even ones
[[nodiscard]] unsigned int textListColor(const Carousel& c, int index, int cursor);
// tint behind a row that is not the selected one
[[nodiscard]] unsigned int textListRowTint(int index);

// Transition between systems and views: the carousel's defaultTransition in the main theme
// when it has one ("fade", "instant", anything else "slide": RecalboxConf::TransitionFromString),
// else the user setting (ThemeData::getThemeTransition, themes/ThemeData.cpp:668-679).
[[nodiscard]] std::string transitionStyle(const ThemeData* mainTheme, const std::string& setting);

// The main theme has a carousel element (`systemcarousel`) or a `systemInfo` text in the
// system view: the system view follows the rules above; otherwise it keeps ES's own layout.
[[nodiscard]] bool isThemed(const ThemeData* mainTheme);

} // namespace es::carousel
