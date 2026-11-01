// Entry icons and section headers of the menus (phase 11f4; docs/THEMES.md "menuIcons" and
// "menuSection"; Recalbox Menu.cpp:38,309-337 and ItemHeader.h:20-22,94-153 @10.1.1).
#include "MenuTheme.h"
#include "ThemeData.h"
#include "Window.h"
#include "components/MenuComponent.h"
#include "theme/ThemeDiagnostics.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

namespace fs = std::filesystem;
using es::theme::MenuIcon;
using es::theme::MenuTheme;

namespace {

const fs::path kDir = ES_TEST_DATA_DIR "/themes/menu";

MenuTheme loadMenuTheme()
{
	ThemeData theme;
	es::theme::Diagnostics diag;
	REQUIRE(theme.loadFile((kDir / "set" / "theme.xml").string(), diag, 1920, 1080));
	return MenuTheme::fromTheme(theme);
}

} // namespace

TEST_CASE("menu icon geometry: side 0.8 x row height, margin 3 + width / 320", "[MenuComponent][11f4]")
{
	// 1280 wide: margin 3 + 4 = 7; row 49.5 high: side (int)39.6 = 39; label moved by 39 + 7
	MenuIconGeometry g = menuIconGeometry(49.5f, 1280.0f);
	CHECK(g.side == 39.0f);
	CHECK(g.x == 7.0f);
	CHECK(g.textOffset == 46.0f);

	// 1920 wide: margin 3 + 6 = 9; row 74 high: side (int)59.2 = 59
	g = menuIconGeometry(74.0f, 1920.0f);
	CHECK(g.side == 59.0f);
	CHECK(g.x == 9.0f);
	CHECK(g.textOffset == 68.0f);

	// 650 wide: integer division, 3 + 2 = 5
	g = menuIconGeometry(30.0f, 650.0f);
	CHECK(g.side == 24.0f);
	CHECK(g.x == 5.0f);
	CHECK(g.textOffset == 29.0f);
}

TEST_CASE("menu icon path: only the icons the theme gives", "[MenuComponent][11f4]")
{
	const MenuTheme mt = loadMenuTheme();
	CHECK(menuIconPath(mt, MenuIcon::UI) == (kDir / "set" / "art" / "ui.png").generic_string());
	CHECK(menuIconPath(mt, MenuIcon::Sound) == (kDir / "set" / "art" / "sound.png").generic_string());
	CHECK(menuIconPath(mt, MenuIcon::Quit).empty());
	CHECK(menuIconPath(mt, std::nullopt).empty());
	CHECK(menuIconPath(MenuTheme{}, MenuIcon::UI).empty());
}

TEST_CASE("menu row without a theme icon: the label alone, as before 11f4", "[MenuComponent][11f4]")
{
	Window window;
	auto label = std::make_shared<GuiComponent>(&window);

	ComponentListRow plain;
	addIconAndLabel(plain, &window, label, nullptr, 7.0f);
	REQUIRE(plain.elements.size() == 1);
	CHECK(plain.elements[0].component == label);
	CHECK(plain.elements[0].resize_width);
	CHECK(plain.elements[0].invert_when_selected);

	// with an icon: icon, gap of the margin, label; the label still takes the free width
	auto icon = std::make_shared<GuiComponent>(&window);
	ComponentListRow withIcon;
	addIconAndLabel(withIcon, &window, label, icon, 7.0f);
	REQUIRE(withIcon.elements.size() == 3);
	CHECK(withIcon.elements[0].component == icon);
	CHECK_FALSE(withIcon.elements[0].resize_width);
	CHECK(withIcon.elements[0].invert_when_selected);
	CHECK(withIcon.elements[1].component->getSize().x == 7.0f);
	CHECK_FALSE(withIcon.elements[1].resize_width);
	CHECK(withIcon.elements[2].component == label);
	CHECK(withIcon.elements[2].resize_width);
}

TEST_CASE("section header text: capitals and open mark", "[MenuComponent][11f4]")
{
	CHECK(sectionHeaderText("Probes configuration", ALIGN_LEFT) == "PROBES CONFIGURATION\xE2\x96\xBC ");
	CHECK(sectionHeaderText("Probes configuration", ALIGN_CENTER) == "\xE2\x96\xBC PROBES CONFIGURATION");
	CHECK(sectionHeaderText("Probes configuration", ALIGN_RIGHT) == "\xE2\x96\xBC PROBES CONFIGURATION");
}

TEST_CASE("section header lines: free side of the label, gap width / 160", "[MenuComponent][11f4]")
{
	// row 600 wide, label 200 wide, 1280x720: gap 8, line 2 px, text 10 px from its edge
	SectionHeaderLayout l = sectionHeaderLayout(600.0f, 200.0f, ALIGN_LEFT, 1280.0f, 720.0f);
	CHECK(l.lineHeight == 2);
	CHECK(l.textMargin == 10.0f);
	REQUIRE(l.lineCount == 1);
	CHECK(l.lines[0] == glm::vec2(218.0f, 382.0f));

	l = sectionHeaderLayout(600.0f, 200.0f, ALIGN_RIGHT, 1280.0f, 720.0f);
	REQUIRE(l.lineCount == 1);
	CHECK(l.lines[0] == glm::vec2(2.0f, 382.0f));

	// centered: (600 - 200) / 2 - 10 = 190 on each side
	l = sectionHeaderLayout(600.0f, 200.0f, ALIGN_CENTER, 1280.0f, 720.0f);
	REQUIRE(l.lineCount == 2);
	CHECK(l.lines[0] == glm::vec2(2.0f, 190.0f));
	CHECK(l.lines[1] == glm::vec2(408.0f, 190.0f));

	// 480p or lower: 1 px line; 640 wide: gap 4
	l = sectionHeaderLayout(300.0f, 100.0f, ALIGN_CENTER, 640.0f, 480.0f);
	CHECK(l.lineHeight == 1);
	CHECK(l.textMargin == 6.0f);
	CHECK(l.lines[0] == glm::vec2(2.0f, 94.0f));
	CHECK(l.lines[1] == glm::vec2(204.0f, 94.0f));
}

TEST_CASE("themed selector bar: set by the selected text, the selector or the background", "[MenuTheme][11f4]")
{
	MenuTheme mt;
	CHECK_FALSE(mt.themedSelector());
	mt.text.color = 0x112233FF;
	CHECK_FALSE(mt.themedSelector());
	mt.backgroundColor = 0x445566FF;
	CHECK(mt.themedSelector());
	CHECK(loadMenuTheme().themedSelector());
}
