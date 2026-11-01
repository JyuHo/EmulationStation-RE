#pragma once

// Phase 11 skeleton (PROMPT.md section 14). Layer 2 of the theme engine (plan, 11.3): the
// declarative table of the element types and of the type of each of their properties, and the
// conversion of a raw string into a typed value. Replaces ThemeData::sElementMap.

#include "math/Math.h"
#include "theme/ThemeDiagnostics.h"

#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace es::theme {

enum class PropertyType
{
	NormalizedPair, // "0.25 0.5"
	Path,			// one path; "./" relative to the declaring file, "~" expanded
	String,
	Color, // RRGGBB or RRGGBBAA, stored as 0xRRGGBBAA
	Float,
	Boolean // true/false/1/0
};

// Same alternatives and order as ThemeData::ThemeElement::properties: the existing
// get<glm::vec2>/get<std::string>/get<unsigned int>/get<float>/get<bool> calls of the components
// keep working unchanged. Recalbox 10.1.1 has no list of paths (docs/THEMES.md section 11).
using PropertyValue = std::variant<glm::vec2, std::string, unsigned int, float, bool>;

struct PropertySpec
{
	std::string_view name;
	PropertyType type;
};

struct ElementSpec
{
	std::string_view type;					  // XML node name
	std::span<const PropertySpec> properties; // every property this element accepts
	bool canBeExtra = false;				  // may be created by extra="true"
};

// Every element type known to the program (format 3 and Recalbox), exactly the tables of
// docs/THEMES.md section 11 (checked by test_ThemeSchema.cpp). nullptr: unknown type (the loader's caller warns and skips it).
[[nodiscard]] const ElementSpec* findElement(std::string_view type);

// Every element of the table, in the order of docs/THEMES.md section 11.
[[nodiscard]] std::span<const ElementSpec> allElements();

// nullptr: this element does not accept this property (warning, property skipped).
[[nodiscard]] const PropertySpec* findProperty(const ElementSpec& element, std::string_view name);

// Converts a raw value. `declaringFile` is the file that wrote the value ("./" paths are relative
// to its folder). The error string is an English message without location: the caller adds it.
// Number parsing goes through parseFloat (Util.h, std::from_chars), never atof/stof (0.3).
[[nodiscard]] std::expected<PropertyValue, std::string> parseProperty(
	PropertyType type, std::string_view raw, const std::filesystem::path& declaringFile);

} // namespace es::theme
