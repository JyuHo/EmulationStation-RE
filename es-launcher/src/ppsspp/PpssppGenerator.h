// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.ppsspp), Copyright (c) 2026 The Batocera Team.

#pragma once

#include "es-launcher/Generator.h"
#include "util/IniFile.h"

#include <filesystem>

namespace es::launcher::ppsspp
{
	// Configures PPSSPP (PSP). Registered as "ppsspp".
	//
	// PPSSPP's memory stick is $XDG_CONFIG_HOME/ppsspp: ES points XDG_CONFIG_HOME at
	// ctx.paths.configFor("ppsspp"). Its settings are in PSP/SYSTEM; PSP/SAVEDATA and
	// PSP/PPSSPP_STATE are links to <saves>/psp, PSP/SCREENSHOT to ES's screenshots.
	//
	// Options are the ones of batocera-launch ("internal_resolution", "texture_scaling_level"...),
	// and "ppsspp.<section>.<key>" sets any key of ppsspp.ini.
	class PpssppGenerator : public Generator
	{
	public:
		Command configure(const LaunchContext& ctx) override;

		static std::filesystem::path memoryStick(const LaunchContext& ctx);

		// The pieces configure() is made of, public for the tests
		static void writeSettings(const LaunchContext& ctx, util::IniFile& ini);
		static void writeControls(const LaunchContext& ctx, util::IniFile& controls);
	};
}
