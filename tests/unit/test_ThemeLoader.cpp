// Phase 11 (PROMPT.md section 14). Hidden until PR 11b: remove "[]" from every tag of this file
// in that PR, never weaken an expected value (rule 14.2-R7). Fixtures: tests/data/themes/recalbox/
// (own content, never a copy of a Recalbox theme).

#include "theme/ThemeLoader.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>
#include <string>

using namespace es::theme;
namespace fs = std::filesystem;

namespace {

const fs::path kDir = fs::path(ES_TEST_DATA_DIR) / "themes" / "recalbox";

LoaderContext context(std::string language = "en", std::string region = "eu",
	std::map<std::string, std::string, std::less<>> options = {})
{
	Diagnostics ignored;
	LoaderContext ctx;
	ctx.legacy = {.system = "nes", .language = language, .country = ""};
	ctx.region = std::move(region);
	ctx.variables.set("root", kDir.generic_string(), ignored, {});
	ctx.variables.set("system.name", "nes", ignored, {});
	ctx.conditions = [](std::string_view id) -> std::optional<bool> {
		if(id == "hd" || id == "fhd")
			return true;
		if(id == "tate" || id == "crt")
			return false;
		return std::nullopt;
	};
	ctx.selectedOptions = std::move(options);
	return ctx;
}

const RawElement* find(const LoadedTheme& theme, std::string_view view, std::string_view name)
{
	auto it = theme.views.find(view);
	if(it == theme.views.end())
		return nullptr;
	for(const RawElement& e : it->second.elements)
		if(e.name == name)
			return &e;
	return nullptr;
}

const RawProperty* prop(const RawElement& element, std::string_view name)
{
	for(const RawProperty& p : element.properties)
		if(p.name == name)
			return &p;
	return nullptr;
}

std::string value(const LoadedTheme& theme, std::string_view view, std::string_view element, std::string_view name)
{
	const RawElement* e = find(theme, view, element);
	if(!e)
		return "<no element>";
	const RawProperty* p = prop(*e, name);
	return p ? p->value : "<no property>";
}

} // namespace

TEST_CASE("Loader: root attributes and format version", "[phase11][11b][ThemeLoader]")
{
	Diagnostics d;
	const auto theme = loadTheme(kDir / "theme.xml", context(), d);
	REQUIRE(theme.has_value());
	CHECK(theme->formatVersion == 4.0f);
	CHECK(theme->info.name == "Fixture");
	CHECK(theme->info.author == "EmulationStation-RE");
	CHECK(theme->info.version == "1.2");
	CHECK(theme->info.recalboxVersion == 10);
	CHECK(theme->info.compatibility == std::vector<std::string>{"hdmi", "crt"});
	CHECK(theme->info.resolutions == std::vector<std::string>{"fhd", "hd"});
	CHECK(theme->files.front() == kDir / "theme.xml");
}

TEST_CASE("Loader: includes, variables and merging", "[phase11][11b][ThemeLoader]")
{
	Diagnostics d;
	const auto theme = loadTheme(kDir / "theme.xml", context(), d);
	REQUIRE(theme.has_value());

	// ${root}/systems/${system.name}.xml, "./" kept raw, declaring file recorded
	const RawElement* logo = find(*theme, "system", "logo");
	REQUIRE(logo != nullptr);
	REQUIRE(prop(*logo, "path") != nullptr);
	CHECK(prop(*logo, "path")->value == "./../art/nes.svg");
	CHECK(prop(*logo, "path")->file == kDir / "systems" / "nes.xml");

	// user variable referring to another one
	CHECK(value(*theme, "detailed", "background", "color") == "2e447c80");
	// "detailed, basic" then "detailed": properties merged, both views exist
	CHECK(value(*theme, "detailed", "background", "path") == "./art/background.png");
	CHECK(value(*theme, "detailed", "background", "tile") == "true");
	CHECK(value(*theme, "basic", "background", "path") == "./art/background.png");
	CHECK(value(*theme, "basic", "background", "tile") == "<no property>");

	// a missing include is a located warning, not a failure
	const bool warnedMissing = std::ranges::any_of(d.all(), [](const Diagnostic& x) {
		return x.severity == Severity::Warning && x.message.find("missing.xml") != std::string::npos &&
			x.where.file == kDir / "theme.xml" && x.where.line == 8;
	});
	CHECK(warnedMissing);
	CHECK_FALSE(d.hasErrors());

	// every file read, for the hot reload
	CHECK(std::ranges::count(theme->files, kDir / "variables.xml") == 1);
}

TEST_CASE("Loader: element order and locations", "[phase11][11b][ThemeLoader]")
{
	Diagnostics d;
	const auto theme = loadTheme(kDir / "theme.xml", context(), d);
	REQUIRE(theme.has_value());
	// includes are read before the views of the including file (format 3 and Recalbox):
	// md_description (colors/blue.xml), gamelist (layout/compact.xml), then theme.xml
	const RawView& detailed = theme->views.at("detailed");
	auto index = [&](std::string_view name) {
		return std::ranges::find(detailed.elements, name, &RawElement::name) - detailed.elements.begin();
	};
	CHECK(index("md_description") < index("gamelist"));
	CHECK(index("gamelist") < index("background"));
	CHECK(index("background") < index("e_hdOnly"));
	CHECK(index("e_hdOnly") < index("e_title"));

	// location of the first declaration
	const RawElement* background = find(*theme, "detailed", "background");
	REQUIRE(background != nullptr);
	CHECK(background->where == SourceLocation{kDir / "theme.xml", 10});
	const RawElement* description = find(*theme, "detailed", "md_description");
	REQUIRE(description != nullptr);
	CHECK(description->where == SourceLocation{kDir / "colors" / "blue.xml", 3});
	const RawElement* title = find(*theme, "detailed", "e_title");
	REQUIRE(title != nullptr);
	CHECK(title->extra);
	CHECK(title->type == "text");
}

TEST_CASE("Loader: conditions", "[phase11][11b][ThemeLoader]")
{
	Diagnostics d;
	const auto theme = loadTheme(kDir / "theme.xml", context(), d);
	REQUIRE(theme.has_value());
	CHECK(find(*theme, "detailed", "e_hdOnly") != nullptr);
	CHECK(find(*theme, "detailed", "e_tateOnly") == nullptr);
	CHECK(find(*theme, "detailed", "e_exists") != nullptr);
	CHECK(find(*theme, "detailed", "e_notExists") == nullptr);
	// syntax error: node skipped, warning at its line
	CHECK(find(*theme, "detailed", "e_badCondition") == nullptr);
	const bool warned = std::ranges::any_of(
		d.all(), [](const Diagnostic& x) { return x.where == SourceLocation{kDir / "theme.xml", 28}; });
	CHECK(warned);
}

TEST_CASE("Loader: regions and localized properties", "[phase11][11b][ThemeLoader]")
{
	{
		Diagnostics d;
		const auto eu = loadTheme(kDir / "theme.xml", context("en", "eu"), d);
		REQUIRE(eu.has_value());
		CHECK(value(*eu, "detailed", "logo", "path") == "./art/logo-eu.png");
		CHECK(value(*eu, "detailed", "e_title", "text") == "Hello");
	}
	{
		Diagnostics d;
		const auto jp = loadTheme(kDir / "theme.xml", context("fr", "jp"), d);
		REQUIRE(jp.has_value());
		CHECK(value(*jp, "detailed", "logo", "path") == "./art/logo-jp.png");
		// the localized property wins whatever the declaration order
		CHECK(value(*jp, "detailed", "e_title", "text") == "Bonjour");
	}
}

TEST_CASE("Loader: subsets and options", "[phase11][11b][ThemeLoader]")
{
	Diagnostics d;
	const auto theme = loadTheme(kDir / "theme.xml", context("fr"), d);
	REQUIRE(theme.has_value());

	const auto colorset = std::ranges::find(theme->subsets, std::string("colorset"), &ThemeSubset::id);
	REQUIRE(colorset != theme->subsets.end());
	CHECK(colorset->title == "Couleurs");
	CHECK(colorset->help == "Choisir les couleurs");
	REQUIRE(colorset->choices.size() == 2); // "crt only" fails its condition
	CHECK(colorset->choices[0].name == "blue");
	CHECK(colorset->choices[0].displayName == "bleu");
	CHECK(colorset->choices[1].name == "red");

	const auto layout = std::ranges::find(theme->subsets, std::string("layout"), &ThemeSubset::id);
	REQUIRE(layout != theme->subsets.end());
	CHECK(layout->title == "layout"); // no <subset> node: the id
	REQUIRE(layout->choices.size() == 1);

	// nothing selected: first option of each subset
	CHECK(value(*theme, "detailed", "md_description", "color") == "0000FFFF");
	CHECK(value(*theme, "detailed", "gamelist", "size") == "0.4 0.8");
	// options merge with the rest of the element
	CHECK(value(*theme, "detailed", "md_description", "fontSize") == "0.02");

	Diagnostics d2;
	const auto red = loadTheme(kDir / "theme.xml", context("en", "eu", {{"colorset", "red"}}), d2);
	REQUIRE(red.has_value());
	CHECK(value(*red, "detailed", "md_description", "color") == "FF0000FF");

	// a stored option that does not exist any more: first option
	Diagnostics d3;
	const auto stale = loadTheme(kDir / "theme.xml", context("en", "eu", {{"colorset", "green"}}), d3);
	REQUIRE(stale.has_value());
	CHECK(value(*stale, "detailed", "md_description", "color") == "0000FFFF");
}

TEST_CASE("Loader: header only", "[phase11][11b][ThemeLoader]")
{
	Diagnostics d;
	const auto header = loadThemeHeader(kDir / "theme.xml", context(), d);
	REQUIRE(header.has_value());
	CHECK(header->info.name == "Fixture");
	CHECK(header->subsets.size() == 2);
	CHECK(header->views.empty());
}

TEST_CASE("Loader: failures", "[phase11][11b][ThemeLoader]")
{
	{
		Diagnostics d;
		CHECK_FALSE(loadTheme(kDir / "does-not-exist.xml", context(), d).has_value());
		CHECK(d.hasErrors());
	}
	{
		Diagnostics d;
		CHECK_FALSE(loadTheme(kDir / "broken.xml", context(), d).has_value());
		REQUIRE(d.hasErrors());
		CHECK(d.all().back().where.file == kDir / "broken.xml");
		CHECK(d.all().back().where.line > 0);
	}
	{
		// an include cycle is cut with a warning, the rest is loaded
		Diagnostics d;
		const auto theme = loadTheme(kDir / "cycle-a.xml", context(), d);
		REQUIRE(theme.has_value());
		CHECK(value(*theme, "basic", "background", "path") == "./art/background.png");
		CHECK(d.warningCount() >= 1);
		CHECK_FALSE(d.hasErrors());
	}
}

TEST_CASE("Loader: format 3 themes still load", "[phase11][11b][ThemeLoader]")
{
	// the capture theme of tests/smoke/capture.sh (format 3, includes, ./ paths)
	const fs::path dir = fs::path(ES_TEST_DATA_DIR) / "capture" / "themes" / "capture";
	Diagnostics d;
	const auto theme = loadTheme(dir / "nes" / "theme.xml", context(), d);
	REQUIRE(theme.has_value());
	CHECK(theme->formatVersion == 3.0f);
	CHECK(theme->info.name.empty());
	CHECK(theme->subsets.empty());
	CHECK(value(*theme, "system", "logo", "path") == "./../art/logo-nes.svg");
	CHECK(d.all().empty());
}
