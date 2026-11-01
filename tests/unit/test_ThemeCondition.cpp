// Phase 11 (PROMPT.md section 14). Hidden until PR 11b: remove "[]" from every tag of this file
// in that PR, never weaken an expected value (rule 14.2-R7).

#include "theme/ThemeCondition.h"

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>

using namespace es::theme;

namespace {

const SourceLocation kWhere{"theme.xml", 12};

ConditionResolver resolver(std::map<std::string, bool, std::less<>> values)
{
	return [values = std::move(values)](std::string_view id) -> std::optional<bool> {
		auto it = values.find(id);
		if(it == values.end())
			return std::nullopt;
		return it->second;
	};
}

bool eval(std::string_view expression, Diagnostics& diagnostics)
{
	return evaluateCondition(
		expression, resolver({{"t", true}, {"f", false}, {"hd", true}, {"tate", false}}), diagnostics, kWhere);
}

} // namespace

TEST_CASE("Conditions: identifiers and operators", "[phase11][11b][ThemeCondition]")
{
	Diagnostics d;
	CHECK(eval("t", d));
	CHECK_FALSE(eval("f", d));
	CHECK(eval("!f", d));
	CHECK(eval("not f", d));
	CHECK(eval("t & t", d));
	CHECK_FALSE(eval("t and f", d));
	CHECK(eval("f | t", d));
	CHECK(eval("f or t", d));
	CHECK(eval("hd and !tate", d));
	CHECK_FALSE(eval("!(t | f)", d));
	CHECK(eval("(f | t) & !(f)", d));
	CHECK(eval("  t\t&\tt  ", d));
	CHECK(d.all().empty());
}

TEST_CASE("Conditions: left to right, no precedence (Recalbox 10.1.1)", "[phase11][11b][ThemeCondition]")
{
	Diagnostics d;
	// "t | f & f" is "(t | f) & f", not "t | (f & f)"
	CHECK_FALSE(eval("t | f & f", d));
	// "f & f | t" is "(f & f) | t"
	CHECK(eval("f & f | t", d));
	CHECK(d.all().empty());
}

TEST_CASE("Conditions: unknown identifier is false with a warning", "[phase11][11b][ThemeCondition]")
{
	Diagnostics d;
	CHECK_FALSE(eval("crt", d));
	REQUIRE(d.all().size() == 1);
	CHECK(d.all()[0].severity == Severity::Warning);
	CHECK(d.all()[0].where == kWhere);
	CHECK(d.all()[0].message.find("crt") != std::string::npos);

	// the unknown identifier is false, so its negation is true (and warns again)
	CHECK(eval("!crt", d));
	CHECK(d.all().size() == 2);
}

TEST_CASE("Conditions: empty expression is true with a warning", "[phase11][11b][ThemeCondition]")
{
	Diagnostics d;
	CHECK(eval("", d));
	CHECK(eval("   ", d));
	CHECK(d.warningCount() == 2);
}

TEST_CASE("Conditions: syntax errors are false with a warning", "[phase11][11b][ThemeCondition]")
{
	for(const char* bad : {"t &", "& t", "t t", "(t", "t)", "t # t", "!", "()"})
	{
		CAPTURE(bad);
		Diagnostics d;
		CHECK_FALSE(eval(bad, d));
		REQUIRE(d.warningCount() == 1);
		CHECK(d.all()[0].message.find(bad) != std::string::npos);
	}
}

TEST_CASE("Conditions: identifiers of this program", "[phase11][11b][ThemeCondition]")
{
	// 11b: complete this test from the identifier table of docs/THEMES.md (one CHECK per
	// identifier listed there, resolution thresholds included). The checks below are the minimum.
	// fields assigned one by one: 11b may add fields to ConditionEnvironment
	ConditionEnvironment fhdScreen;
	fhdScreen.screenWidth = 1920;
	fhdScreen.screenHeight = 1080;
	fhdScreen.systemName = "nes";
	const auto fhd = makeConditionResolver(fhdScreen);
	CHECK(fhd("fhd") == std::optional<bool>(true));
	CHECK(fhd("crt") == std::optional<bool>(false));
	CHECK(fhd("jamma") == std::optional<bool>(false));
	CHECK(fhd("ispc") == std::optional<bool>(true));
	CHECK(fhd("ispi") == std::optional<bool>(false));
	CHECK(fhd("nonsense") == std::nullopt);

	ConditionEnvironment qvgaScreen;
	qvgaScreen.screenWidth = 320;
	qvgaScreen.screenHeight = 240;
	const auto small = makeConditionResolver(qvgaScreen);
	CHECK(small("fhd") == std::optional<bool>(false));
	CHECK(small("hd") == std::optional<bool>(false));
}
