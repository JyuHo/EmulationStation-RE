// Phase 11g: themed system carousel, values and placement without rendering
// (docs/THEMES.md, element `carousel`). Fixtures: tests/data/themes/carousel/ (own content).

#include "views/SystemCarousel.h"

#include "ThemeData.h"
#include "theme/ThemeDiagnostics.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <memory>

using namespace es::carousel;
using Catch::Approx;
namespace fs = std::filesystem;

namespace {

const fs::path kDir = ES_TEST_DATA_DIR "/themes/carousel";
const glm::vec2 kScreen(1280.0f, 720.0f);

std::shared_ptr<ThemeData> load(const fs::path& file)
{
	auto theme = std::make_shared<ThemeData>();
	es::theme::Diagnostics diag;
	REQUIRE(theme->loadFile(file.string(), diag, 1280, 720));
	return theme;
}

Carousel horizontal()
{
	Carousel c = defaults(kScreen);
	c.maxLogoCount = 3;
	return c;
}

} // namespace

TEST_CASE("Carousel: values without a theme are Recalbox's", "[phase11][11g][SystemCarousel]")
{
	const Carousel c = fromElement(nullptr, kScreen);
	CHECK(c.type == Type::Horizontal);
	CHECK(c.size.x == Approx(1280.0f));
	CHECK(c.size.y == Approx(0.2325f * 720.0f));
	CHECK(c.pos.x == Approx(0.0f));
	CHECK(c.pos.y == Approx(0.5f * (720.0f - 0.2325f * 720.0f)));
	CHECK(c.logoSize.x == Approx(0.25f * 1280.0f));
	CHECK(c.logoSize.y == Approx(0.155f * 720.0f));
	CHECK(c.color == 0xFFFFFFD8);
	CHECK(c.logoScale == Approx(1.2f));
	CHECK(c.logoRotation == Approx(7.5f));
	CHECK(c.logoRotationOrigin == glm::vec2(-5.0f, 0.5f));
	CHECK(c.maxLogoCount == 3);
	CHECK(c.zIndex == Approx(40.0f));
	CHECK(c.fontColor == 0xE6E6E6FF);
	CHECK_FALSE(c.textOnly);
	CHECK(c.selectorColor == 0xFFFFFF33);
	CHECK(c.selectedColor == 0xFFFFFFFF);
	CHECK(c.primaryColor == 0xE6E6E6FF);
	CHECK(c.secondaryColor == 0xC6C6C6FF);
	CHECK(c.lineSpacing == Approx(1.5f));
	CHECK_FALSE(c.fontPath.has_value());
	CHECK_FALSE(c.defaultTransition.has_value());
}

TEST_CASE("Carousel: every property of the element", "[phase11][11g][SystemCarousel]")
{
	const auto theme = load(kDir / "full.xml");
	const Carousel c = fromElement(theme->getElement("system", "systemcarousel", "carousel"), kScreen);
	CHECK(c.type == Type::VerticalWheel);
	CHECK(c.size == glm::vec2(640.0f, 180.0f));
	CHECK(c.pos == glm::vec2(128.0f, 144.0f));
	CHECK(c.origin == glm::vec2(0.5f, 0.0f));
	CHECK(c.color == 0x11223344);
	REQUIRE(c.fontSize.has_value());
	CHECK(*c.fontSize == Approx(0.05f));
	CHECK(c.fontColor == 0x55667788);
	CHECK(c.logoScale == Approx(2.0f));
	CHECK(c.logoRotation == Approx(10.0f));
	CHECK(c.logoRotationOrigin == glm::vec2(-2.0f, 0.5f));
	CHECK(c.logoSize.x == Approx(128.0f));
	CHECK(c.logoSize.y == Approx(144.0f));
	CHECK(c.textOnly);
	CHECK(c.forceUppercase);
	CHECK(c.hAlign == HAlign::Right);
	CHECK(c.vAlign == VAlign::Center);
	CHECK(c.maxLogoCount == 5); // rounded
	CHECK(c.selectorColor == 0x010203FF);
	CHECK(c.selectedColor == 0x040506FF);
	CHECK(c.primaryColor == 0x070809FF);
	CHECK(c.secondaryColor == 0x0A0B0CFF);
	CHECK(c.selectorHeight == Approx(72.0f));
	CHECK(c.selectorOffsetX == Approx(12.8f));
	CHECK(c.selectorOffsetY == Approx(14.4f));
	CHECK(c.textOffsetX == Approx(38.4f));
	CHECK(c.lineSpacing == Approx(2.0f));
	CHECK(c.horizontalMargin == Approx(64.0f));
	CHECK(c.zIndex == Approx(60.0f));
	REQUIRE(c.defaultTransition.has_value());
	CHECK(*c.defaultTransition == "instant");
}

TEST_CASE("Carousel: type and alignment names", "[phase11][11g][SystemCarousel]")
{
	CHECK(parseType("vertical") == Type::Vertical);
	CHECK(parseType("vertical_wheel") == Type::VerticalWheel);
	CHECK(parseType("horizontal") == Type::Horizontal);
	CHECK(parseType("anything") == Type::Horizontal);

	HAlign h = HAlign::Center;
	VAlign v = VAlign::Center;
	parseAlignment("TopRight", h, v);
	CHECK((h == HAlign::Right && v == VAlign::Top));
	parseAlignment("bottom", h, v);
	CHECK((h == HAlign::Center && v == VAlign::Bottom));
	parseAlignment("left", h, v);
	CHECK((h == HAlign::Left && v == VAlign::Center));
	parseAlignment("unknown", h, v); // Recalbox: center left
	CHECK((h == HAlign::Left && v == VAlign::Center));
}

TEST_CASE("Carousel: horizontal placement", "[phase11][11g][SystemCarousel]")
{
	const Carousel c = horizontal();
	const float spacing = (1280.0f - 320.0f * 3) / 3 + 320.0f;
	// selected logo in the middle, vertically centered
	const glm::vec2 sel = slotPosition(c, 2.0f, 2);
	CHECK(sel.x == Approx((1280.0f - 320.0f) / 2));
	CHECK(sel.y == Approx((c.size.y - c.logoSize.y) / 2));
	CHECK(slotPosition(c, 2.0f, 3).x == Approx(sel.x + spacing));
	CHECK(slotPosition(c, 2.0f, 1).x == Approx(sel.x - spacing));
	// halfway between two logos
	CHECK(slotPosition(c, 2.5f, 2).x == Approx(sel.x - spacing / 2));

	Carousel top = c;
	top.vAlign = VAlign::Top;
	CHECK(slotPosition(top, 0.0f, 0).y == Approx(c.logoSize.y / 10));
	CHECK(logoOrigin(top) == glm::vec2(0.5f, 0.0f));
	Carousel bottom = c;
	bottom.vAlign = VAlign::Bottom;
	CHECK(slotPosition(bottom, 0.0f, 0).y == Approx(c.size.y - c.logoSize.y * 1.1f));
	CHECK(logoOrigin(bottom) == glm::vec2(0.5f, 1.0f));
	CHECK(logoOrigin(c) == glm::vec2(0.5f, 0.5f));
}

TEST_CASE("Carousel: vertical and wheel placement", "[phase11][11g][SystemCarousel]")
{
	Carousel c = horizontal();
	c.type = Type::Vertical;
	c.size = {400.0f, 720.0f};
	c.logoSize = {200.0f, 100.0f};
	c.maxLogoCount = 4;
	const float spacing = (720.0f - 100.0f * 4) / 4 + 100.0f;
	const glm::vec2 sel = slotPosition(c, 1.0f, 1);
	CHECK(sel.y == Approx((720.0f - 100.0f) / 2));
	CHECK(sel.x == Approx((400.0f - 200.0f) / 2));
	CHECK(slotPosition(c, 1.0f, 2).y == Approx(sel.y + spacing));
	c.hAlign = HAlign::Left;
	CHECK(slotPosition(c, 1.0f, 1).x == Approx(20.0f));
	CHECK(logoOrigin(c) == glm::vec2(0.0f, 0.5f));
	c.hAlign = HAlign::Right;
	CHECK(slotPosition(c, 1.0f, 1).x == Approx(400.0f - 220.0f));
	CHECK(logoOrigin(c) == glm::vec2(1.0f, 0.5f));
	CHECK(logoRotationAt(c, 1.0f) == Approx(0.0f));

	// wheel: every logo on the same slot, turned by logoRotation per slot of distance
	c.type = Type::VerticalWheel;
	c.hAlign = HAlign::Center;
	CHECK(slotPosition(c, 1.0f, 0) == slotPosition(c, 1.0f, 3));
	CHECK(slotPosition(c, 1.0f, 0).y == Approx((720.0f - 100.0f) / 2));
	CHECK(logoRotationAt(c, 0.0f) == Approx(0.0f));
	CHECK(logoRotationAt(c, -2.0f) == Approx(-15.0f));
}

TEST_CASE("Carousel: scale, opacity, visible logos, wrapping", "[phase11][11g][SystemCarousel]")
{
	const Carousel c = horizontal(); // logoScale 1.2
	CHECK(logoScaleFactor(c, 0.0f) == Approx(1.0f));
	CHECK(logoScaleFactor(c, 1.0f) == Approx(1.0f / 1.2f));
	CHECK(logoScaleFactor(c, -3.0f) == Approx(1.0f / 1.2f));
	CHECK(logoScaleFactor(c, 0.5f) == Approx(1.1f / 1.2f));
	CHECK(logoOpacity(0.0f) == 0xFF);
	CHECK(logoOpacity(1.0f) == 0x80);
	CHECK(logoOpacity(5.0f) == 0x80);
	CHECK(logoOpacity(0.5f) == 0xC0);

	CHECK(visibleRange(c, 0.0f, 10) == glm::ivec2(-2, 2));
	CHECK(visibleRange(c, 4.6f, 10) == glm::ivec2(2, 6));
	CHECK(visibleRange(c, 0.0f, 2) == glm::ivec2(-2, 2)); // min(3, 2) = 2 logos
	CHECK(visibleRange(c, 0.0f, 1) == glm::ivec2(0, 0));

	CHECK(wrapIndex(-1, 5) == 4);
	CHECK(wrapIndex(-6, 5) == 4);
	CHECK(wrapIndex(5, 5) == 0);
	CHECK(wrapIndex(7, 5) == 2);
	CHECK(wrapIndex(3, 0) == 0);
}

TEST_CASE("Carousel: text list", "[phase11][11g][SystemCarousel]")
{
	Carousel c = horizontal();
	c.textOnly = true;
	c.size = {600.0f, 300.0f};
	c.lineSpacing = 1.5f;
	// font height 20 -> rows of 30 px, 10 per screen
	TextListLayout l = textListLayout(c, 20.0f, 0, 40);
	CHECK(l.rowHeight == Approx(30.0f));
	CHECK(l.startOffset == Approx(0.0f));
	CHECK(l.selectorY == Approx(0.0f));
	CHECK(l.selectorHeight == Approx(30.0f));
	CHECK(l.rowCount == 13);
	// cursor in the middle of the screen
	l = textListLayout(c, 20.0f, 20, 40);
	CHECK(l.startOffset == Approx(15.0f));
	CHECK(l.firstRow == 15);
	CHECK(l.selectorY == Approx(150.0f));
	// near the end: the last row stays at the bottom
	l = textListLayout(c, 20.0f, 39, 40);
	CHECK(l.startOffset == Approx(30.0f));
	// fewer systems than rows: from the top
	l = textListLayout(c, 20.0f, 3, 5);
	CHECK(l.startOffset == Approx(0.0f));
	CHECK(l.selectorY == Approx(90.0f));

	c.selectorHeight = 12.0f;
	c.selectorOffsetY = 4.0f;
	l = textListLayout(c, 20.0f, 1, 5);
	CHECK(l.selectorHeight == Approx(12.0f));
	CHECK(l.selectorY == Approx(34.0f));

	CHECK(textListColor(c, 2, 2) == c.selectedColor);
	CHECK(textListColor(c, 3, 2) == c.secondaryColor);
	CHECK(textListColor(c, 4, 2) == c.primaryColor);
	CHECK(textListRowTint(1) == 0xFFFFFF08);
	CHECK(textListRowTint(2) == 0x00000008);
}

TEST_CASE("Carousel: themed view and default transition", "[phase11][11g][SystemCarousel]")
{
	CHECK_FALSE(isThemed(nullptr));
	CHECK(transitionStyle(nullptr, "fade") == "fade");

	const auto minimal = load(ES_TEST_DATA_DIR "/themes/minimal.xml");
	CHECK_FALSE(isThemed(minimal.get()));
	CHECK(transitionStyle(minimal.get(), "slide") == "slide");

	const auto full = load(kDir / "full.xml");
	CHECK(isThemed(full.get()));
	CHECK(transitionStyle(full.get(), "fade") == "instant");

	const auto info = load(kDir / "info.xml"); // systemInfo alone
	CHECK(isThemed(info.get()));
	CHECK(transitionStyle(info.get(), "fade") == "fade");

	const auto other = load(kDir / "other-transition.xml"); // unknown value: slide
	CHECK(transitionStyle(other.get(), "fade") == "slide");
}
