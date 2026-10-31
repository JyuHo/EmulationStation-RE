#pragma once

#include <string>
#include <Eigen/Dense>
#include <filesystem>
#include <chrono>
#include <optional>

std::string strToUpper(const char* from);
std::string& strToUpper(std::string& str);
std::string strToUpper(const std::string& str);

Eigen::Affine3f& roundMatrix(Eigen::Affine3f& mat);
Eigen::Affine3f roundMatrix(const Eigen::Affine3f& mat);

Eigen::Vector3f roundVector(const Eigen::Vector3f& vec);
Eigen::Vector2f roundVector(const Eigen::Vector2f& vec);

#if defined(_WIN32) && _MSC_VER < 1800
float round(float num);
#endif

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

std::optional<std::chrono::sys_seconds> string_to_ptime(const std::string& str, const std::string& fmt = "%Y%m%dT%H%M%S");
