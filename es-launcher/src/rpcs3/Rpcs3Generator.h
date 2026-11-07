// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch-rpcs3, Copyright (c) 2026 The Batocera Team.

#pragma once

#include "es-launcher/Generator.h"
#include "util/YamlFile.h"

#include <filesystem>

namespace es::launcher::rpcs3
{
	// Configures RPCS3 (PlayStation 3, x86_64 only). Registered as "rpcs3".
	//
	// XDG_CONFIG_HOME is ctx.paths.configFor("rpcs3"), so RPCS3 keeps its files in its rpcs3/
	// folder there; the PS3 hard disk (dev_hdd0: saves, installed PSN games and updates) is
	// <saves>/ps3/dev_hdd0. A game is a disc folder (PS3_GAME/USRDIR/EBOOT.BIN inside), an .iso,
	// or a .psn file that holds the id of a game installed in dev_hdd0.
	//
	// The PS3 firmware is installed from PS3UPDAT.PUP (BIOS folder, or its ps3/ folder) before
	// the first game; without either, the launch stops with a message.
	//
	// Options are the ones of batocera-launch ("rpcs3_gfxbackend", "rpcs3_ppudecoder"...).
	class Rpcs3Generator : public Generator
	{
	public:
		Command configure(const LaunchContext& ctx) override;

		// Public for the tests
		static std::filesystem::path configDirectory(const LaunchContext& ctx);
		static void writeSettings(const LaunchContext& ctx, util::YamlFile& config);
		static util::YamlFile inputConfig(const LaunchContext& ctx);
		static util::YamlFile vfsConfig(const LaunchContext& ctx);
	};
}
