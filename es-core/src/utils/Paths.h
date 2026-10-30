#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace es::paths {

enum class Kind { Config, Data, State, Cache };

std::filesystem::path userDir(Kind kind);
std::filesystem::path userFile(Kind kind, std::string_view rel);
std::vector<std::filesystem::path> systemDirs(Kind kind);
std::optional<std::filesystem::path> find(Kind kind, std::string_view rel);
std::filesystem::path expandUser(std::string_view path);

// Portable / test mode
void setHome(const std::filesystem::path& home);
void clearHome();

// Migration from ~/.emulationstation (best-effort, idempotent)
void migrateFromLegacy();

} // namespace es::paths