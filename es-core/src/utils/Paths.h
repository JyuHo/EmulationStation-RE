#pragma once

#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

// The only place that knows where files live. Follows the XDG Base Directory
// specification; see docs/MODERNIZATION_PLAN.md, phase 3.A.
namespace es::paths {

enum class Kind
{
	Config, // $XDG_CONFIG_HOME/emulationstation: es_settings.cfg, es_input.cfg, es_systems.cfg
	Data,	// $XDG_DATA_HOME/emulationstation: themes/, gamelists/, downloaded_images/
	State,	// $XDG_STATE_HOME/emulationstation: es_log.txt
	Cache	// $XDG_CACHE_HOME/emulationstation
};

// The user's home directory ($HOME, or the password database when $HOME is unset or relative).
// Not affected by setHome(): "~" in es_systems.cfg always means the real home.
std::filesystem::path homeDir();

// User directory for kind, created with mode 0700 if missing.
std::filesystem::path userDir(Kind kind);
std::filesystem::path userFile(Kind kind, std::string_view rel);

// Read-only system directories for kind, highest priority first: $XDG_CONFIG_DIRS or $XDG_DATA_DIRS,
// then the installation prefix, then <executable>/../share/emulationstation for Data.
std::vector<std::filesystem::path> systemDirs(Kind kind);

// Directory of the running executable (from /proc/self/exe), e.g. build/release in a build tree.
std::optional<std::filesystem::path> executableDir();

// User directory first, then systemDirs(kind).
std::optional<std::filesystem::path> find(Kind kind, std::string_view rel);

// Expands a leading "~" or "~/" to homeDir().
std::filesystem::path expandUser(std::string_view path);

// Portable mode (--home / ES_HOME): every user directory becomes <home>/{config,data,state,cache}/emulationstation.
void setHome(const std::filesystem::path& home);
void clearHome();
bool hasHomeOverride();

// One-time copy of ~/.emulationstation into the XDG directories. Never moves, deletes or overwrites
// a file. Skipped in portable mode. Returns the files copied.
std::vector<std::filesystem::path> migrateFromLegacy();

} // namespace es::paths
