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
