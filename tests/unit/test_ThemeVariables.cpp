// Phase 11 (PROMPT.md section 14). Hidden until PR 11b: remove "[.]" from every tag of this file
// in that PR, never weaken an expected value (rule 14.2-R7).

#include "theme/ThemeDiagnostics.h"
#include "theme/ThemeVariables.h"

#include <catch2/catch_test_macros.hpp>

using namespace es::theme;

namespace {
const SourceLocation kWhere{"variables.xml", 3};
}

TEST_CASE("Variables: names", "[.][phase11][11b][ThemeVariables]")
{
	CHECK(ThemeVariables::isValidName("Primary"));
	CHECK(ThemeVariables::isValidName("_x1"));
	CHECK(ThemeVariables::isValidName("system.name"));
	CHECK(ThemeVariables::isValidName("a.b.c"));
	CHECK_FALSE(ThemeVariables::isValidName(""));
	CHECK_FALSE(ThemeVariables::isValidName("1abc"));
	CHECK_FALSE(ThemeVariables::isValidName("a..b"));
	CHECK_FALSE(ThemeVariables::isValidName("a-b"));
	CHECK_FALSE(ThemeVariables::isValidName("a b"));

	Diagnostics d;
	ThemeVariables vars;
	vars.set("bad name", "x", d, kWhere);
	CHECK_FALSE(vars.get("bad name").has_value());
	REQUIRE(d.warningCount() == 1);
	CHECK(d.all()[0].where == kWhere);
}

TEST_CASE("Variables: resolution", "[.][phase11][11b][ThemeVariables]")
{
	Diagnostics d;
	ThemeVariables vars;
	vars.set("Primary", "2e447c", d, kWhere);
	vars.set("Alpha", "FF", d, kWhere);
	vars.set("PrimaryAlpha", "${Primary}${Alpha}", d, kWhere);
	vars.set("root", "/themes/next", d, kWhere);

	CHECK(vars.get("Primary") == std::optional<std::string_view>("2e447c"));
	CHECK(vars.resolve("${Primary}", d, kWhere) == "2e447c");
	CHECK(vars.resolve("${PrimaryAlpha}", d, kWhere) == "2e447cFF");
	CHECK(vars.resolve("${ Primary }", d, kWhere) == "2e447c"); // spaces trimmed (Recalbox)
	CHECK(vars.resolve("${root}/data/${Primary}.png", d, kWhere) == "/themes/next/data/2e447c.png");
	CHECK(vars.resolve("no variable", d, kWhere) == "no variable");
	// unknown names are kept as written, without warning (resolved later or never)
	CHECK(vars.resolve("${game.name} ${Primary}", d, kWhere) == "${game.name} 2e447c");
	CHECK(vars.resolve("${unterminated", d, kWhere) == "${unterminated");
	CHECK(d.all().empty());

	// redefinition replaces
	vars.set("Primary", "000000", d, kWhere);
	CHECK(vars.resolve("${Primary}", d, kWhere) == "000000");
}

TEST_CASE("Variables: cycles do not hang", "[.][phase11][11b][ThemeVariables]")
{
	Diagnostics d;
	ThemeVariables vars;
	vars.set("a", "x${b}", d, kWhere);
	vars.set("b", "y${a}", d, kWhere);
	vars.set("self", "${self}", d, kWhere);

	const std::string a = vars.resolve("${a}", d, kWhere);
	CHECK(a.find("${") == std::string::npos);
	CHECK(a.size() < 1024);
	CHECK(d.warningCount() >= 1);

	Diagnostics d2;
	CHECK(vars.resolve("[${self}]", d2, kWhere) == "[]");
	CHECK(d2.warningCount() == 1);
}

TEST_CASE("Variables: legacy substitutions", "[.][phase11][11b][ThemeVariables]")
{
	const LegacySubstitutions values{.system = "nes", .language = "fr", .country = "FR"};
	CHECK(applyLegacySubstitutions("./$system/logo.png", values) == "./nes/logo.png");
	CHECK(applyLegacySubstitutions("./$language/$country.png", values) == "./fr/FR.png");
	CHECK(applyLegacySubstitutions("./art/logo.png", values) == "./art/logo.png");
	// 11b: add the cases of every substitution listed in docs/THEMES.md (fallback of $system
	// included if the documentation keeps it).
}
