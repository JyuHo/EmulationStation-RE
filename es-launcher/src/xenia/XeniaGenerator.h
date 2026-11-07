// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.xenia_edge), Copyright (c) 2026 The Batocera Team.

#pragma once

#include "es-launcher/Generator.h"
#include "util/IniFile.h"

#include <filesystem>

namespace es::launcher::xenia
{
	// Configures Xenia Edge, the native Linux build of Xenia (Xbox 360, x86_64 only, Vulkan).
	// Registered as "xenia-edge".
	//
	// Xenia keeps its files in $XDG_DATA_HOME/Xenia: ES points XDG_DATA_HOME at
	// ctx.paths.configFor("xenia-edge"), so the settings are in <that>/Xenia/xenia-edge.config.toml.
	// Game saves and installed content go to <saves>/xbox360.
	//
	// A .xbox360 file is a playlist: its first line names the game to start, relative to it.
	// Options are the ones of batocera-launch ("xenia_edge_resolution_scale"...).
	class XeniaGenerator : public Generator
	{
	public:
		Command configure(const LaunchContext& ctx) override;

		static std::filesystem::path dataDirectory(const LaunchContext& ctx);

		// Public for the tests
		static void writeSettings(const LaunchContext& ctx, util::IniFile& toml);
		static std::filesystem::path resolvePlaylist(const std::filesystem::path& rom);
	};
}
