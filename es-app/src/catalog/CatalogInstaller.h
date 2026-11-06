#pragma once

// Downloads and installs what the emulator catalog offers, without root:
//
//   AppImage and archive emulators  $XDG_DATA_HOME/emulationstation/emulators/<id>/
//   Flatpak emulators               flatpak install --user
//   libretro cores and .info files  $XDG_DATA_HOME/emulationstation/cores/
//   what ES installed               $XDG_DATA_HOME/emulationstation/emulators/installed.json
//
// installed.json is also how es-launcher finds what the catalog installed.
// Install and uninstall calls are made from the ContentInstaller thread, one at a time.

#include "catalog/EmulatorCatalog.h"

#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class CatalogInstaller
{
public:
	using Progress = std::function<void(const std::string&)>;
	using Result = std::pair<std::string, int>; // message, 0 on success (like ApiSystem)

	// es_settings: global.catalog.url overrides the default catalog
	static std::string catalogUrl();
	static std::string emulatorsPath();
	static std::string coresPath();
	static std::string installedPath();

	// Downloads catalog.json. Falls back to the copy of the last successful download when
	// offline, so that what is installed can still be listed and removed.
	static std::optional<catalog::Catalog> fetchCatalog(std::string* error = nullptr);

	static catalog::Installed loadInstalled();

	static bool isFlatpakAvailable();
	static bool isFoundOnSystem(const catalog::Emulator& emulator); // in PATH, or a Flatpak installed outside ES
	static bool isCoreFoundOnSystem(const catalog::Core& core);     // in a libretro directory of the distribution

	static catalog::State state(const catalog::Emulator& emulator, const catalog::Installed& installed);
	static catalog::State state(const catalog::Core& core, const catalog::Installed& installed);

	static Result installEmulator(const std::string& id, const Progress& progress = nullptr);
	static Result uninstallEmulator(const std::string& id, const Progress& progress = nullptr);
	static Result installCore(const std::string& id, const Progress& progress = nullptr);
	static Result uninstallCore(const std::string& id, const Progress& progress = nullptr);

private:
	static std::optional<catalog::Catalog> currentCatalog(std::string* error);
	static bool saveInstalled(const catalog::Installed& installed);
	static bool download(const std::string& url, const std::string& file, const std::string& label, const Progress& progress);

	static std::mutex mLock; // guards installed.json and the cached catalog
	static std::optional<catalog::Catalog> mCatalog;
};
