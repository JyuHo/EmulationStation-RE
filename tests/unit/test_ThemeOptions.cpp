// Phase 11 (PROMPT.md section 14). Hidden until PR 11b (selection, needed by the loader) and
// PR 11e (settings key): remove "[]" from the tags of each case in its PR, never weaken an expected value (rule 14.2-R7).

#include "theme/ThemeOptions.h"

#include <catch2/catch_test_macros.hpp>

using namespace es::theme;

namespace {
ThemeSubset subset(std::string id, std::vector<std::string> names)
{
	ThemeSubset s;
	s.id = id;
	s.title = id;
	for(auto& n : names)
		s.choices.push_back({.name = n, .displayName = n, .where = {}});
	return s;
}
} // namespace

TEST_CASE("Options: settings key", "[phase11][11e][ThemeOptions]")
{
	CHECK(optionSettingKey("recalbox-next", "systemView") == "ThemeOption.recalbox-next.systemView");
}

TEST_CASE("Options: selection", "[phase11][11b][ThemeOptions]")
{
	const std::vector<ThemeSubset> subsets = {
		subset("colorset", {"blue", "red"}), subset("layout", {"compact", "large"}), subset("empty", {})};

	auto none = resolveSelectedOptions(subsets, {});
	CHECK(none.size() == 2);
	CHECK(none.at("colorset") == "blue");
	CHECK(none.at("layout") == "compact");
	CHECK_FALSE(none.contains("empty"));

	auto stored = resolveSelectedOptions(subsets, {{"colorset", "red"}, {"layout", "gone"}, {"old", "x"}});
	CHECK(stored.at("colorset") == "red");
	CHECK(stored.at("layout") == "compact"); // stale choice
	CHECK_FALSE(stored.contains("old"));	 // stale subset
}

namespace {
std::vector<std::string> names(const std::vector<ThemeOptionChoice>& choices)
{
	std::vector<std::string> out;
	for(const auto& c : choices)
		out.push_back(c.name);
	return out;
}
} // namespace

TEST_CASE("Options: order of the values in the menu", "[ThemeOptions][11e2]")
{
	// Recalbox ThemeOption.h:112-122, :141-150: numeric when every value starts with an index
	CHECK(names(sortOptionChoices(subset("s", {"10 - b", "2 - a", "1 - c"}).choices)) ==
		  std::vector<std::string>{"1 - c", "2 - a", "10 - b"});
	CHECK(names(sortOptionChoices(subset("s", {"red", "blue", "green"}).choices)) ==
		  std::vector<std::string>{"blue", "green", "red"});
	// mixed: alphabetical
	CHECK(names(sortOptionChoices(subset("s", {"red", "2 - a", "10 - b"}).choices)) ==
		  std::vector<std::string>{"10 - b", "2 - a", "red"});
	CHECK(sortOptionChoices({}).empty());
}

TEST_CASE("Options: value shown without its index", "[ThemeOptions][11e2]")
{
	// Recalbox ThemeOption.h:26-41
	CHECK(displayOptionName("2 - Rouge") == "Rouge");
	CHECK(displayOptionName("10 - Horizontal") == "Horizontal");
	CHECK(displayOptionName("Rouge") == "Rouge");
	CHECK(displayOptionName("2") == "2");
}
