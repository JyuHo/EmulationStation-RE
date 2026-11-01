// On-screen keyboard (phase 11f5; docs/THEMES.md "keyboard"): layout and input without rendering,
// theme view, colors.
#include "MenuTheme.h"
#include "ThemeData.h"
#include "guis/GuiVirtualKeyboard.h"
#include "guis/VirtualKeyboardModel.h"
#include "theme/ThemeDiagnostics.h"
#include "views/ViewController.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

using Model = VirtualKeyboardModel;

namespace {

// moves to the first key with this label (lower case) or kind
void select(Model& m, Model::KeyKind kind, const std::string& label = {})
{
	for(int guard = 0; guard < 100; guard++)
	{
		const auto& k = m.selected();
		if(k.kind == kind && (label.empty() || k.label == label))
			return;
		m.move(1, 0);
		if(m.column() == 0)
			m.move(0, 1);
	}
	FAIL("key not found");
}

} // namespace

TEST_CASE("virtual keyboard: moves wrap around on each edge", "[VirtualKeyboard][11f5]")
{
	Model m;
	REQUIRE(m.rows().size() == 5);
	CHECK(m.row() == 0);
	CHECK(m.column() == 0);
	m.move(-1, 0); // left of the first key: the last one of the row
	CHECK(m.column() == 9);
	m.move(1, 0);
	CHECK(m.column() == 0);
	for(int r = 0; r < 4; r++)
	{
		CAPTURE(r);
		CHECK(m.rows()[r].size() == 10);
	}
	CHECK(m.rows()[4].size() == 5);
	m.move(0, -1); // above the first row: the last row
	CHECK(m.row() == 4);
	CHECK(m.selected().kind == Model::KeyKind::Shift);
	m.move(-1, 0);
	CHECK(m.selected().kind == Model::KeyKind::Cancel);
	m.move(0, 1); // below the last row: the first one, under the key center (units 8-9)
	CHECK(m.row() == 0);
	CHECK(m.column() == 8);
	// down from "4" (unit 3): Space covers units 2-3
	m.move(-5, 0);
	CHECK(m.column() == 3);
	m.move(0, -1);
	CHECK(m.selected().kind == Model::KeyKind::Space);
}

TEST_CASE("virtual keyboard: letters, digits, space, delete", "[VirtualKeyboard][11f5]")
{
	Model m;
	select(m, Model::KeyKind::Char, "q");
	CHECK(m.press() == Model::Result::None);
	select(m, Model::KeyKind::Char, "7");
	m.press();
	select(m, Model::KeyKind::Space);
	m.press();
	CHECK(m.text() == "q7 ");
	select(m, Model::KeyKind::Backspace);
	m.press();
	CHECK(m.text() == "q7");
	m.backspace();
	m.backspace();
	m.backspace(); // empty: nothing
	CHECK(m.text().empty());
	select(m, Model::KeyKind::Ok);
	CHECK(m.press() == Model::Result::Ok);
	select(m, Model::KeyKind::Cancel);
	CHECK(m.press() == Model::Result::Cancel);
}

TEST_CASE("virtual keyboard: shift once, locked, off", "[VirtualKeyboard][11f5]")
{
	Model m;
	select(m, Model::KeyKind::Shift);
	m.press();
	CHECK(m.shift() == Model::Shift::Once);
	select(m, Model::KeyKind::Char, "a");
	CHECK(m.displayLabel(m.selected()) == "A");
	m.press();
	m.press(); // the shift applied to one letter only
	CHECK(m.text() == "Aa");
	CHECK(m.shift() == Model::Shift::Off);

	select(m, Model::KeyKind::Shift);
	m.press();
	m.press();
	CHECK(m.shift() == Model::Shift::Locked);
	select(m, Model::KeyKind::Char, "b");
	m.press();
	m.press();
	select(m, Model::KeyKind::Char, "-"); // not a letter: unchanged
	m.press();
	CHECK(m.text() == "AaBB-");
	select(m, Model::KeyKind::Shift);
	m.press();
	CHECK(m.shift() == Model::Shift::Off);
}

TEST_CASE("virtual keyboard: UTF-8 text deleted by character", "[VirtualKeyboard][11f5]")
{
	Model m("Pokémon 日本");
	CHECK(m.text() == "Pokémon 日本");
	m.backspace();
	CHECK(m.text() == "Pokémon 日");
	m.backspace();
	m.backspace();
	m.backspace();
	m.backspace();
	m.backspace();
	CHECK(m.text() == "Poké");
	m.backspace();
	CHECK(m.text() == "Pok");
}

TEST_CASE("virtual keyboard: theme view under the keyboard", "[VirtualKeyboard][11f5]")
{
	CHECK(ViewController::keyboardThemeView(ViewController::SYSTEM_SELECT, "detailed") == "system");
	CHECK(ViewController::keyboardThemeView(ViewController::GAME_LIST, "detailed") == "detailed");
	CHECK(ViewController::keyboardThemeView(ViewController::GAME_LIST, "basic") == "basic");
	CHECK(ViewController::keyboardThemeView(ViewController::START_SCREEN, "") == "menu");
	CHECK(ViewController::keyboardThemeView(ViewController::NOTHING, "") == "menu");
}

TEST_CASE("virtual keyboard: colors of the theme view, else those of Recalbox", "[VirtualKeyboard][11f5]")
{
	const GuiVirtualKeyboard::Colors none = GuiVirtualKeyboard::colorsFrom({});
	CHECK(none.key == 0x303030F0);
	CHECK(none.keySelected == 0xC02020FF);
	CHECK(none.keyText == 0xFFFFFFFF);
	CHECK(none.keyDisabled == 0x707070FF);
	CHECK(none.keyModifier == 0x4488FFFF);
	CHECK(none.keyModifierLocked == 0x224488FF);
	CHECK(none.title == 0xFFFF80FF);
	CHECK(none.editText == 0xFFFFFFFF);

	ThemeData theme;
	es::theme::Diagnostics diag;
	REQUIRE(theme.loadFile(ES_TEST_DATA_DIR "/themes/menu/keyboard.xml", diag, 1920, 1080));
	const auto detailed = GuiVirtualKeyboard::colorsFrom(es::theme::KeyboardTheme::fromTheme(theme, "detailed"));
	CHECK(detailed.key == 0x112233FF);
	CHECK(detailed.keySelected == 0x445566FF);
	CHECK(detailed.title == 0x778899FF);
	CHECK(detailed.keyText == 0xFFFFFFFF); // absent: Recalbox's
	const auto system = GuiVirtualKeyboard::colorsFrom(es::theme::KeyboardTheme::fromTheme(theme, "system"));
	CHECK(system.key == 0x303030F0);
	CHECK(system.keySelected == 0xC02020FF);
}
