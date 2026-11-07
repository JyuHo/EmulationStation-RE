// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.duckstation), Copyright (c) 2026 The Batocera Team.

#pragma once

#include "es-launcher/Generator.h"
#include "util/IniFile.h"

#include <filesystem>
#include <map>
#include <string>

namespace es::launcher::duckstation
{
	// Configures DuckStation (PlayStation). Registered as "duckstation".
	//
	// DuckStation keeps everything in $XDG_CONFIG_HOME/duckstation: ES points XDG_CONFIG_HOME at
	// ctx.paths.configFor("duckstation"), so settings.ini is in <that>/duckstation/settings.ini.
	// Memory cards and save states go to <saves>/psx, screenshots to ES's folder.
	//
	// Options are the ones of batocera-launch ("duckstation_gfxbackend", "duckstation_ratio"...).
	class DuckStationGenerator : public Generator
	{
	public:
		Command configure(const LaunchContext& ctx) override;

		static std::filesystem::path dataDirectory(const LaunchContext& ctx);

		// The BIOS found in dir for each region ("NTSCU", "PAL", "NTSCJ", "Uni"), matched without
		// case: region -> file name.
		static std::map<std::string, std::string> findBios(const std::filesystem::path& dir);

		// The pieces configure() is made of, public for the tests. Throw LaunchError.
		static void writeSettings(const LaunchContext& ctx, util::IniFile& settings);
		static void writeControllers(const LaunchContext& ctx, util::IniFile& settings);
	};
}
