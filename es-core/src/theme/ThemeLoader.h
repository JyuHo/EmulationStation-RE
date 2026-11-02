#pragma once

// Phase 11 skeleton (PROMPT.md section 14). Layer 1 of the theme engine (plan, 11.3): reads the
// XML files of a theme, follows <include>, applies variables, conditions, options (subsets),
// regions and localized attributes, and returns a normalized tree of views and elements whose
// property values are still raw strings. No GL, no Settings, no ResourceManager: everything the
// loader depends on comes in through LoaderContext, so that it is tested without a window.

#include "theme/ThemeCondition.h"
#include "theme/ThemeDiagnostics.h"
#include "theme/ThemeVariables.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace es::theme {

struct LoaderContext
{
	LegacySubstitutions legacy;	  // $system, $language, $country
	std::string region;			  // "us", "eu" or "jp": filters the `region` attribute
	ThemeVariables variables;	  // variables set by the program (${root}, ${system.name}...)
	ConditionResolver conditions; // `if` identifiers; empty: every identifier is unknown

	// Option chosen for each subset: subset id -> option name (the `name` attribute of the
	// <include>). A subset absent from this map uses its first available option (14.5).
	std::map<std::string, std::string, std::less<>> selectedOptions;
};

struct RawProperty
{
	std::string name;  // "pos", "path"... without the localization suffix
	std::string value; // after variables and legacy substitutions; "./" paths NOT resolved yet
	SourceLocation where;
	std::filesystem::path file; // file that declared it: "./" paths are relative to its folder
};

struct RawElement
{
	std::string type; // XML node name: "image", "text", "carousel"...
	std::string name; // one name per element: "md_image" (lists of names are split)
	bool extra = false;
	SourceLocation where;				 // first declaration
	std::vector<RawProperty> properties; // one entry per property name, last declaration wins
};

struct RawView
{
	// Declaration order of the first occurrence of each name (extras are drawn in this order).
	// Redefining an element merges its properties into the first one (format 3 rule).
	std::vector<RawElement> elements;
};

struct ThemeOptionChoice
{
	std::string name;		 // value stored in the settings: the `name` attribute
	std::string displayName; // localized `name.<lang>` if present, otherwise `name`
	SourceLocation where;
};

struct ThemeSubset
{
	std::string id;							// the `subset` attribute: "systemView", "colorset"...
	std::string title;						// localized `title` of <subset>, or the id when there is none
	std::string help;						// localized `help` of <subset>, may be empty
	std::vector<ThemeOptionChoice> choices; // declaration order, only those whose `if` holds
};

// Attributes of the root <theme> node (Recalbox format; all empty for a format 3 theme).
struct ThemeInfo
{
	std::string name;
	std::string author;
	std::string version;
	int recalboxVersion = 0; // major version of the "recalbox" attribute: "10" and "10.1" give 10
	std::vector<std::string> compatibility; // "hdmi", "crt", "jamma", "tate"
	std::vector<std::string> resolutions;	// "qvga", "vga", "hd", "fhd"
};

struct LoadedTheme
{
	float formatVersion = 0.0f; // <formatVersion> of the main file
	ThemeInfo info;
	std::vector<ThemeSubset> subsets;				   // every option of the theme, for the "Theme options" menu
	std::map<std::string, std::string, std::less<>> selectedOptions; // result of resolveSelectedOptions
	std::map<std::string, RawView, std::less<>> views; // lowercase view name -> view
	std::vector<std::filesystem::path> files;		   // every file read, main file first (hot reload)
	bool usesRegion = false; // true if any element carries a region attribute
};

// Loads `file` and everything it includes.
// Returns std::nullopt only when the main file cannot be used (missing, not XML, no <theme>,
// no or unsupported <formatVersion>): an Error diagnostic says why, and the caller falls back
// to the default theme. Any other problem (bad include, unknown attribute, cycle, failed
// condition syntax...) is a Warning and the faulty node is skipped.
[[nodiscard]] std::optional<LoadedTheme> loadTheme(
	const std::filesystem::path& file, const LoaderContext& context, Diagnostics& diagnostics);

// Features of <feature supported="..."> whose views are read: "carousel" and "z-index"
// (Recalbox ThemeSupport::SupportedFeatures, themes/ThemeSupport.cpp:166-173 @10.1.1).
[[nodiscard]] bool isSupportedFeature(std::string_view feature);

// Reads only the root attributes and the subsets of a theme (following its includes, but
// keeping no view): used by the theme list and the "Theme options" menu. Same failure rules as loadTheme().
[[nodiscard]] std::optional<LoadedTheme> loadThemeHeader(
	const std::filesystem::path& file, const LoaderContext& context, Diagnostics& diagnostics);

} // namespace es::theme
