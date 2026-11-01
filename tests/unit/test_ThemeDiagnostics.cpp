// Phase 11 (PROMPT.md section 14). Hidden until PR 11b: remove "[.]" from every tag of this file
// in that PR, never weaken an expected value (rule 14.2-R7).

#include "theme/ThemeDiagnostics.h"

#include <catch2/catch_test_macros.hpp>

using namespace es::theme;

TEST_CASE("Diagnostics: locations", "[.][phase11][11b][ThemeDiagnostics]")
{
	CHECK(toString({"themes/next/theme.xml", 7}) == "themes/next/theme.xml:7");
	CHECK(toString({"themes/next/theme.xml", 0}) == "themes/next/theme.xml");
}

TEST_CASE("Diagnostics: collection", "[.][phase11][11b][ThemeDiagnostics]")
{
	Diagnostics d;
	CHECK_FALSE(d.hasErrors());
	d.warn({"a.xml", 1}, "first");
	d.warn({"a.xml", 2}, "second");
	CHECK_FALSE(d.hasErrors());
	CHECK(d.warningCount() == 2);
	d.error({"b.xml", 0}, "fatal");
	CHECK(d.hasErrors());
	REQUIRE(d.all().size() == 3);
	CHECK(d.all()[0].message == "first"); // insertion order
	CHECK(d.all()[2].severity == Severity::Error);
}

TEST_CASE("Diagnostics: line index", "[.][phase11][11b][ThemeDiagnostics]")
{
	const LineIndex index("<theme>\n  <view>\r\n\n</theme>");
	CHECK(index.lineAt(0) == 1);
	CHECK(index.lineAt(7) == 1); // the '\n' belongs to line 1
	CHECK(index.lineAt(8) == 2);
	CHECK(index.lineAt(17) == 2); // "\r\n": the '\n' ends line 2
	CHECK(index.lineAt(18) == 3); // empty line
	CHECK(index.lineAt(19) == 4);
	CHECK(index.lineAt(-1) == 0);
	CHECK(index.lineAt(1000) == 0);

	const LineIndex empty("");
	CHECK(empty.lineAt(0) == 0);
}
