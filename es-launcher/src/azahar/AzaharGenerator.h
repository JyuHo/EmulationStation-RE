// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.azahar), Copyright (c) 2026 The Batocera Team.

#pragma once

#include "es-launcher/Generator.h"
#include "util/IniFile.h"

#include <filesystem>

namespace es::launcher::azahar
{
	// Configures Azahar (Nintendo 3DS). Registered as "azahar".
	//
	// Azahar's settings are in $XDG_CONFIG_HOME/azahar-emu/qt-config.ini, with XDG_CONFIG_HOME set
	// to ctx.paths.configFor("azahar"). Its data (the SD card, the NAND, saves) is in
	// $XDG_DATA_HOME/azahar-emu, with XDG_DATA_HOME set to <saves>/3ds.
	//
	// Player 1's pad is bound input by input, from the mapping made in ES. Options are the ones
	// of batocera-launch ("azahar_screen_layout", "azahar_resolution_factor"...).
	class AzaharGenerator : public Generator
	{
	public:
		Command configure(const LaunchContext& ctx) override;

		static std::filesystem::path settingsPath(const LaunchContext& ctx);

		// Public for the tests
		static void writeSettings(const LaunchContext& ctx, util::IniFile& ini);
	};
}
