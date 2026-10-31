#pragma once

#include <string>
#include "math/Math.h"
#include <filesystem>
#include <chrono>
#include <optional>
#include <string_view>

std::string strToUpper(const char* from);
std::string& strToUpper(std::string& str);
std::string strToUpper(const std::string& str);

std::string getCanonicalPath(const std::string& str);

// example: removeCommonPath("$HOME/roms/nes/foo/bar.nes", "$HOME/roms/nes/") returns "foo/bar.nes"
std::filesystem::path removeCommonPath(
	const std::filesystem::path& path, const std::filesystem::path& relativeTo, bool& contains);

// usage: makeRelativePath("/path/to/my/thing.sfc", "/path/to") -> "./my/thing.sfc"
// usage: makeRelativePath("$HOME/my/thing.sfc", "/path/to", true) -> "~/my/thing.sfc"
std::filesystem::path makeRelativePath(
	const std::filesystem::path& path, const std::filesystem::path& relativeTo, bool allowHome);

// expands "./my/path.sfc" to "[relativeTo]/my/path.sfc"
// if allowHome is true, also expands "~/my/path.sfc" to "$HOME/my/path.sfc"
std::filesystem::path resolvePath(
	const std::filesystem::path& path, const std::filesystem::path& relativeTo, bool allowHome);

// Takes the user's locale for everything (LC_ALL from the environment) except numbers: LC_NUMERIC stays "C" so
// that printf/strtod-style functions, in our code and in libraries such as pugixml, keep using '.' as decimal
// separator in configuration, theme and gamelist files.
void applyUserLocale();

// Locale-independent number parsing (std::from_chars). Like atof/atoi: leading spaces and '+' are skipped,
// trailing characters are ignored, and fallback is returned when no number starts the string.
float parseFloat(std::string_view str, float fallback = 0.0f);
int parseInt(std::string_view str, int fallback = 0);

std::optional<std::chrono::sys_seconds> string_to_ptime(const std::string& str, const std::string& fmt = "%Y%m%dT%H%M%S");
