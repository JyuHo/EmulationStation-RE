#pragma once

// The emulator catalog: what can be installed on this machine, and what already is.
//
// This file only models and parses. It reads catalog.json, published by the es-re-catalog
// repository (https://github.com/JyuHo/es-re-catalog, format documented in its README), and
// installed.json, which records what ES installed. It does no I/O, so it is unit-tested on its
// own; CatalogInstaller does the downloads and the installs.

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace catalog
{
	constexpr int FORMAT = 1;

	enum class BuildType { AppImage, Archive, Flatpak };

	struct Build
	{
		BuildType   type = BuildType::AppImage;
		std::string url;      // appimage, archive
		std::string version;  // appimage, archive
		std::string sha256;   // appimage, archive
		uint64_t    size = 0; // appimage, archive, in bytes
		std::string exec;     // archive: regex of the program inside it
		std::string ref;      // flatpak: application id
		std::string remote;   // flatpak: remote name, flathub by default
	};

	struct Emulator
	{
		std::string id;       // emulator name of es-launcher and es_systems.cfg
		std::string name;
		std::string description;
		std::string homepage;
		std::string license;
		std::string logo;     // absolute URL, empty if none
		std::vector<std::string> systems;
		std::vector<std::string> binaries; // executable names that mean "installed by the system"
		std::vector<Build> builds;          // for this machine's architecture, most preferred first
	};

	struct Core
	{
		std::string id;       // core name, as in <core name="...">
		std::string name;
		std::string description;
		std::vector<std::string> systems;
		std::string url;      // zip holding <id>_libretro.so, for this machine's architecture
		std::string version;  // buildbot date
	};

	struct Catalog
	{
		std::vector<Emulator> emulators; // only those with a build for this architecture
		std::vector<Core> cores;         // same
		std::string coreInfoUrl;         // zip of the libretro .info files

		const Emulator* findEmulator(const std::string& id) const;
		const Core* findCore(const std::string& id) const;
	};

	// Parses catalog.json, keeping what runs on `arch` (as uname -m says it). `catalogUrl` is
	// where it was downloaded from: relative logos are resolved against it. Returns nothing,
	// with the reason in `error`, if the document is not a catalog of a format ES knows.
	std::optional<Catalog> parseCatalog(const std::string& json, const std::string& arch, const std::string& catalogUrl, std::string* error = nullptr);

	// What ES installed, as recorded in installed.json
	struct InstalledItem
	{
		std::string type;                 // appimage, archive, flatpak, core
		std::string version;
		std::string path;                 // installed file or directory, empty for flatpak
		std::vector<std::string> command; // how to start the emulator, empty for a core
		std::string ref;                  // flatpak
	};

	struct Installed
	{
		std::map<std::string, InstalledItem> emulators;
		std::map<std::string, InstalledItem> cores;

		static Installed parse(const std::string& json); // empty on any error
		std::string toJson() const;
	};

	enum class State
	{
		Available,       // can be installed
		Installed,       // installed by ES, up to date
		UpdateAvailable, // installed by ES, the catalog has a newer build
		System,          // installed by the system package manager: ES leaves it alone
	};

	// The build ES would install: the first one this machine can handle
	const Build* preferredBuild(const Emulator& emulator, bool flatpakAvailable);

	State emulatorState(const Emulator& emulator, const Installed& installed, bool flatpakAvailable, bool foundInPath);
	State coreState(const Core& core, const Installed& installed, bool foundInSystem);

	std::string typeName(BuildType type);

	// Ids and refs end up in paths and command lines: only these characters are accepted
	bool isSafeId(const std::string& id);

	// Machine architecture, as uname -m reports it (x86_64, aarch64...)
	std::string hostArchitecture();
}
