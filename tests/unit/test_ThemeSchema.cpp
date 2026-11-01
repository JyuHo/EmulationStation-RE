// Phase 11 (PROMPT.md section 14). Hidden until PR 11c: remove "[]" from every tag of this file
// in that PR, never weaken an expected value (rule 14.2-R7).

#include "theme/ThemeSchema.h"

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace es::theme;
namespace fs = std::filesystem;

TEST_CASE("Schema: format 3 elements are all known", "[phase11][11c][ThemeSchema]")
{
	// the table of ThemeData::sElementMap before phase 11: nothing may disappear
	const std::pair<const char*, std::vector<const char*>> format3[] = {
		{"image", {"pos", "size", "maxSize", "origin", "path", "tile", "color"}},
		{"video", {"pos", "size", "maxSize", "origin", "default", "delay", "showSnapshotNoVideo", "showSnapshotDelay"}},
		{"text",
			{"pos", "size", "text", "color", "fontPath", "fontSize", "alignment", "forceUppercase", "lineSpacing"}},
		{"textlist",
			{"pos", "size", "selectorColor", "selectedColor", "primaryColor", "secondaryColor", "fontPath", "fontSize",
				"scrollSound", "alignment", "horizontalMargin", "forceUppercase", "lineSpacing"}},
		{"container", {"pos", "size"}},
		{"ninepatch", {"pos", "size", "path"}},
		{"datetime", {"pos", "size", "color", "fontPath", "fontSize", "forceUppercase"}},
		{"rating", {"pos", "size", "filledPath", "unfilledPath"}},
		{"sound", {"path"}},
		{"helpsystem", {"pos", "textColor", "iconColor", "fontPath", "fontSize"}},
	};
	for(const auto& [type, properties] : format3)
	{
		CAPTURE(type);
		const ElementSpec* element = findElement(type);
		REQUIRE(element != nullptr);
		for(const char* name : properties)
		{
			CAPTURE(name);
			CHECK(findProperty(*element, name) != nullptr);
		}
	}
	CHECK(findElement("image")->canBeExtra);
	CHECK(findElement("text")->canBeExtra);
	CHECK(findElement("nonsense") == nullptr);
	CHECK(findProperty(*findElement("image"), "nonsense") == nullptr);
}

TEST_CASE("Schema: property types of format 3", "[phase11][11c][ThemeSchema]")
{
	CHECK(findProperty(*findElement("image"), "pos")->type == PropertyType::NormalizedPair);
	CHECK(findProperty(*findElement("image"), "path")->type == PropertyType::Path);
	CHECK(findProperty(*findElement("image"), "color")->type == PropertyType::Color);
	CHECK(findProperty(*findElement("image"), "tile")->type == PropertyType::Boolean);
	CHECK(findProperty(*findElement("text"), "text")->type == PropertyType::String);
	CHECK(findProperty(*findElement("text"), "fontSize")->type == PropertyType::Float);
}

TEST_CASE("Schema: Recalbox elements", "[phase11][11c][ThemeSchema]")
{
	// 11c: one CHECK per element type and per property of the table of docs/THEMES.md
	// (written in 11a). The minimum below comes from the plan (11.2).
	for(const char* type : {"carousel", "menuBackground", "menuText"})
	{
		CAPTURE(type);
		CHECK(findElement(type) != nullptr);
	}
	for(const char* name : {"zIndex", "rotation", "rotationOrigin"})
	{
		CAPTURE(name);
		CHECK(findProperty(*findElement("image"), name) != nullptr);
	}
}

TEST_CASE("Schema: value parsing", "[phase11][11c][ThemeSchema]")
{
	const fs::path file = fs::path(ES_TEST_DATA_DIR) / "themes" / "recalbox" / "theme.xml";

	auto pair = parseProperty(PropertyType::NormalizedPair, "0.25 0.5", file);
	REQUIRE(pair.has_value());
	CHECK(std::get<glm::vec2>(*pair) == glm::vec2(0.25f, 0.5f));
	CHECK_FALSE(parseProperty(PropertyType::NormalizedPair, "0.25", file).has_value());
	CHECK_FALSE(parseProperty(PropertyType::NormalizedPair, "a b", file).has_value());

	auto rgb = parseProperty(PropertyType::Color, "2e447c", file);
	REQUIRE(rgb.has_value());
	CHECK(std::get<unsigned int>(*rgb) == 0x2e447cffu);
	auto rgba = parseProperty(PropertyType::Color, "2E447C80", file);
	REQUIRE(rgba.has_value());
	CHECK(std::get<unsigned int>(*rgba) == 0x2e447c80u);
	CHECK_FALSE(parseProperty(PropertyType::Color, "12345", file).has_value());
	CHECK_FALSE(parseProperty(PropertyType::Color, "GGGGGG", file).has_value());

	auto f = parseProperty(PropertyType::Float, "0.045", file);
	REQUIRE(f.has_value());
	CHECK(std::get<float>(*f) == 0.045f);
	CHECK_FALSE(parseProperty(PropertyType::Float, "abc", file).has_value());

	CHECK(std::get<bool>(*parseProperty(PropertyType::Boolean, "true", file)));
	CHECK(std::get<bool>(*parseProperty(PropertyType::Boolean, "1", file)));
	CHECK_FALSE(std::get<bool>(*parseProperty(PropertyType::Boolean, "false", file)));
	CHECK_FALSE(std::get<bool>(*parseProperty(PropertyType::Boolean, "0", file)));

	CHECK(std::get<std::string>(*parseProperty(PropertyType::String, "Hello world", file)) == "Hello world");

	// "./" relative to the declaring file, without "/" replacing the left side (PROMPT.md 0.3)
	auto path = parseProperty(PropertyType::Path, "./art/background.png", file);
	REQUIRE(path.has_value());
	CHECK(std::get<std::string>(*path) == (file.parent_path() / "art" / "background.png").generic_string());
}

TEST_CASE("Schema: parsing ignores the locale", "[phase11][11c][ThemeSchema]")
{
	// test_Locale.cpp sets fr_FR for its own cases; numbers of a theme always use a dot
	const fs::path file = "theme.xml";
	auto f = parseProperty(PropertyType::Float, "1.5", file);
	REQUIRE(f.has_value());
	CHECK(std::get<float>(*f) == 1.5f);
}

TEST_CASE("Schema: matches docs/THEMES.md section 11", "[ThemeSchema]")
{
	// The document is the source (PROMPT.md 14.2-R4): every row of its tables is in the schema
	// with the same type, and the schema has nothing more.
	std::ifstream doc(ES_TEST_DATA_DIR "/../../docs/THEMES.md");
	REQUIRE(doc.is_open());
	const std::map<std::string, PropertyType> types = {{"NormalizedPair", PropertyType::NormalizedPair},
		{"Path", PropertyType::Path}, {"String", PropertyType::String}, {"Color", PropertyType::Color},
		{"Float", PropertyType::Float}, {"Boolean", PropertyType::Boolean}};

	std::vector<std::pair<std::string, std::string>> documented; // element, property
	std::string line, element;
	bool inSection = false;
	while(std::getline(doc, line))
	{
		if(line.starts_with("## "))
			inSection = line.starts_with("## 11.");
		if(!inSection)
			continue;
		if(line.starts_with("#### `"))
		{
			element = line.substr(6, line.find('`', 6) - 6);
			CAPTURE(element);
			CHECK(findElement(element) != nullptr);
			continue;
		}
		if(element.empty() || !line.starts_with("| `"))
			continue;
		const std::string name = line.substr(3, line.find('`', 3) - 3);
		const size_t typeStart = line.find("| ", 3) + 2;
		const std::string type = line.substr(typeStart, line.find(' ', typeStart) - typeStart);
		CAPTURE(element, name, type);
		const ElementSpec* spec = findElement(element);
		REQUIRE(spec != nullptr);
		const PropertySpec* property = findProperty(*spec, name);
		REQUIRE(property != nullptr);
		REQUIRE(types.contains(type));
		CHECK(property->type == types.at(type));
		documented.emplace_back(element, name);
	}
	CHECK(documented.size() > 300);

	const std::set<std::pair<std::string, std::string>> inDoc(documented.begin(), documented.end());
	CHECK(inDoc.size() == documented.size()); // no row written twice
	for(const ElementSpec& spec : allElements())
		for(const PropertySpec& property : spec.properties)
		{
			CAPTURE(spec.type, property.name);
			CHECK(inDoc.contains({std::string(spec.type), std::string(property.name)}));
		}
}
