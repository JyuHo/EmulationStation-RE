// Menu theme read from the "menu" view of the main theme (docs/THEMES.md section 10, "Vue
// menu"; Recalbox MenuThemeData.cpp @10.1.1). Every value is optional: absent = the component
// keeps its current value.
#include "MenuTheme.h"
#include "ThemeData.h"
#include "Window.h"
#include "theme/ThemeDiagnostics.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

namespace fs = std::filesystem;
using es::theme::MenuIcon;
using es::theme::MenuTheme;

namespace {

const fs::path kDir = ES_TEST_DATA_DIR "/themes/menu";

ThemeData load(const fs::path& file)
{
	ThemeData theme;
	es::theme::Diagnostics diag;
	REQUIRE(theme.loadFile(file.string(), diag, 1920, 1080));
	return theme;
}

std::string art(const char* name)
{
	return (kDir / "set" / "art" / name).generic_string();
}

} // namespace

TEST_CASE("MenuTheme: a theme without a menu view leaves every value absent", "[MenuTheme][11f1]")
{
	const MenuTheme mt = MenuTheme::fromTheme(load(ES_TEST_DATA_DIR "/themes/minimal.xml"));
	CHECK_FALSE(mt.height);
	CHECK_FALSE(mt.backgroundPath);
	CHECK_FALSE(mt.fadePath);
	CHECK_FALSE(mt.backgroundColor);
	for(const auto* style : {&mt.title, &mt.footer, &mt.text, &mt.textSmall, &mt.section})
	{
		CHECK_FALSE(style->font);
		CHECK_FALSE(style->color);
		CHECK_FALSE(style->selectedColor);
		CHECK_FALSE(style->selectorColor);
		CHECK_FALSE(style->separatorColor);
	}
	CHECK_FALSE(mt.sectionAlignment);
	CHECK_FALSE(mt.buttonPath);
	CHECK_FALSE(mt.buttonFilledPath);
	CHECK_FALSE(mt.switchOnPath);
	CHECK_FALSE(mt.switchOffPath);
	CHECK_FALSE(mt.sliderKnobPath);
	CHECK(mt.icons.empty());
}

TEST_CASE("MenuTheme: every element of the menu view goes to its field", "[MenuTheme][11f1]")
{
	const MenuTheme mt = MenuTheme::fromTheme(load(kDir / "set" / "theme.xml"));

	CHECK(mt.height == 0.8f);
	CHECK(mt.backgroundPath == art("frame.png"));
	CHECK(mt.fadePath == art("fade.png"));
	CHECK(mt.backgroundColor == 0x10203040u);

	// a font as soon as fontPath or fontSize is there; the other one takes the element's default
	REQUIRE(mt.title.font);
	CHECK(mt.title.font->size == 0.09f);
	CHECK(mt.title.font->path == FONT_PATH_REGULAR);
	CHECK(mt.title.color == 0x11111111u);
	REQUIRE(mt.footer.font);
	CHECK(mt.footer.font->path == art("frame.png"));
	CHECK(mt.footer.font->size == 0.035f); // FONT_SIZE_SMALL
	CHECK(mt.footer.color == 0x22222222u);

	REQUIRE(mt.text.font);
	CHECK(mt.text.font->size == 0.05f);
	CHECK(mt.text.color == 0x33333333u);
	CHECK(mt.text.separatorColor == 0x44444444u);
	CHECK(mt.text.selectorColor == 0x66666666u);

	CHECK(mt.buttonPath == art("button.png"));
	CHECK(mt.buttonFilledPath == art("button_filled.png"));
	CHECK(mt.switchOnPath == art("on.png"));
	CHECK(mt.switchOffPath == art("off.png"));
	CHECK(mt.sliderKnobPath == art("knob.png"));
}

TEST_CASE("MenuTheme: menutextsmall inherits menutext and keeps Recalbox's particularity", "[MenuTheme][11f1]")
{
	const MenuTheme mt = MenuTheme::fromTheme(load(kDir / "set" / "theme.xml"));

	// copy of menutext, then its own color
	REQUIRE(mt.textSmall.font);
	CHECK(mt.textSmall.font->size == 0.05f);
	CHECK(mt.textSmall.color == 0x77777777u);
	CHECK(mt.textSmall.separatorColor == 0x44444444u);
	// MenuThemeData.cpp:72-73: selectorColor of menutextsmall writes the selected color of
	// menutext, and nothing of menutextsmall
	CHECK(mt.text.selectedColor == 0x88888888u);
	CHECK(mt.textSmall.selectedColor == 0x55555555u);
	CHECK(mt.textSmall.selectorColor == 0x66666666u);
}

TEST_CASE("MenuTheme: menusection is a copy of menutext taken before menutextsmall", "[MenuTheme][11f1]")
{
	const MenuTheme mt = MenuTheme::fromTheme(load(kDir / "set" / "theme.xml"));
	CHECK(mt.section.color == 0x99999999u);
	// copied before the particularity of menutextsmall changed text.selectedColor
	CHECK(mt.section.selectedColor == 0x55555555u);
	CHECK(mt.section.separatorColor == 0x44444444u);
	CHECK(mt.sectionAlignment == ALIGN_RIGHT);

	const MenuTheme inherited = MenuTheme::fromTheme(load(kDir / "inherit.xml"));
	// fontSize only: the path is the one of the menutext font
	REQUIRE(inherited.section.font);
	CHECK(inherited.section.font->path == art("frame.png"));
	CHECK(inherited.section.font->size == 0.06f);
	CHECK(inherited.section.color == 0x33333333u);
	CHECK(inherited.sectionAlignment == ALIGN_CENTER); // any value but left and right
	// no menutextsmall: an exact copy of menutext
	REQUIRE(inherited.textSmall.font);
	CHECK(inherited.textSmall.font->path == art("frame.png"));
	CHECK(inherited.textSmall.font->size == 0.045f); // FONT_SIZE_MEDIUM, the menutext default
	CHECK(inherited.textSmall.color == 0x33333333u);
	CHECK(inherited.textSmall.selectedColor == 0x55555555u);
}

TEST_CASE("MenuTheme: menu icons, iconList included", "[MenuTheme][11f1]")
{
	const MenuTheme mt = MenuTheme::fromTheme(load(kDir / "set" / "theme.xml"));
	REQUIRE(mt.icons.size() == 3);
	CHECK(mt.icons.at(MenuIcon::UI) == art("ui.png"));
	CHECK(mt.icons.at(MenuIcon::Sound) == art("sound.png"));
	CHECK(mt.icons.at(MenuIcon::Lists) == art("list.png"));
}

TEST_CASE("MenuTheme: an element with the right name but another type is ignored", "[MenuTheme][11f1]")
{
	ThemeData theme;
	es::theme::Diagnostics diag;
	REQUIRE(theme.loadFile((kDir / "set" / "theme.xml").string(), diag, 1920, 1080));
	CHECK(theme.getElement("menu", "menutextwrongtype", "menuText") == nullptr);
	CHECK(theme.getElement("menu", "menutitle", "text") == nullptr);
}

TEST_CASE("KeyboardTheme: read from the view the keyboard opens over", "[MenuTheme][11f1]")
{
	const ThemeData theme = load(kDir / "set" / "theme.xml");
	const auto system = es::theme::KeyboardTheme::fromTheme(theme, "system");
	CHECK(system.keyColor == 0xAA0000FFu);
	CHECK(system.fontPath == art("frame.png"));
	CHECK_FALSE(system.keyTitleColor);
	const auto menu = es::theme::KeyboardTheme::fromTheme(theme, "menu");
	CHECK(menu.keyColor == 0x00AA00FFu);
	CHECK(menu.keyTitleColor == 0x0000AAFFu);
	CHECK_FALSE(menu.fontPath);
	const auto none = es::theme::KeyboardTheme::fromTheme(theme, "detailed");
	CHECK_FALSE(none.keyColor);
	CHECK_FALSE(none.fontPath);
}

TEST_CASE("loadMainTheme reads <set>/theme.xml only, without a system", "[MenuTheme][11f1]")
{
	const auto main = ThemeData::loadMainTheme(kDir / "set");
	REQUIRE(main != nullptr);
	// not default/theme.xml (Recalbox ThemeManager.cpp:54-74)
	CHECK(MenuTheme::fromTheme(*main).title.color == 0x11111111u);
	// no system: ${system.name} is not set, systems/${system.name}.xml is a missing include (a
	// warning) and the load goes on
	CHECK(main->getElement("menu", "menubg", "menuBackground") != nullptr);

	// a set with only default/theme.xml has no main theme
	CHECK(ThemeData::loadMainTheme(kDir / "nomain") == nullptr);
	CHECK(ThemeData::loadMainTheme(kDir / "does-not-exist") == nullptr);
}

TEST_CASE("Window keeps the main theme and the menu theme read from it", "[MenuTheme][Window][11f1]")
{
	Window window;
	CHECK_FALSE(window.getMenuTheme().backgroundColor);
	CHECK(window.getMainTheme() == nullptr);

	window.setMainTheme(ThemeData::loadMainTheme(kDir / "set"));
	CHECK(window.getMainTheme() != nullptr);
	CHECK(window.getMenuTheme().backgroundColor == 0x10203040u);

	window.setMainTheme(nullptr);
	CHECK(window.getMainTheme() == nullptr);
	CHECK_FALSE(window.getMenuTheme().backgroundColor);
}
