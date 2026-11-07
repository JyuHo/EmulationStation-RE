// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.cemu), Copyright (c) 2026 The Batocera Team.

#pragma once

#include "es-launcher/Generator.h"

#include <filesystem>
#include <string>

namespace pugi
{
	class xml_document;
}

namespace es::launcher::cemu
{
	// Configures Cemu (Wii U, x86_64 only). Registered as "cemu".
	//
	// Cemu's settings are in $XDG_CONFIG_HOME/Cemu (settings.xml, controllerProfiles/), with
	// XDG_CONFIG_HOME set to ctx.paths.configFor("cemu"). Its data is in $XDG_DATA_HOME/Cemu,
	// with XDG_DATA_HOME set to <saves>/wiiu, and the Wii U's storage (saves, updates, DLC) in
	// <saves>/wiiu/mlc01. keys.txt is linked there from the BIOS folder (cemu/keys.txt or
	// keys.txt) when Cemu has none.
	//
	// Pads are read through SDL. Player 1 is the GamePad, the others Pro Controllers, unless
	// "cemu_controller_combination" says otherwise. Options are the ones of batocera-launch.
	class CemuGenerator : public Generator
	{
	public:
		Command configure(const LaunchContext& ctx) override;

		static std::filesystem::path configDirectory(const LaunchContext& ctx);

		// Public for the tests
		static void writeSettings(const LaunchContext& ctx, pugi::xml_document& settings);

		// controllerN.xml for player N + 1
		static std::string controllerProfile(const LaunchContext& ctx, size_t player);
	};
}
