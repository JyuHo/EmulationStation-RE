// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch_libretro.emulator), Copyright (c) 2026 The Batocera Team.

#pragma once

#include "es-launcher/Generator.h"
#include "retroarch/ConfigFile.h"

#include <filesystem>
#include <vector>

namespace es::launcher::retroarch
{
	// Configures RetroArch for a libretro core. Registered as "libretro" (the emulator name in
	// es_systems.cfg) and "retroarch".
	//
	// Everything lives in its own folder, ctx.paths.configFor("retroarch"):
	//   retroarch.cfg                 written at each launch from the player's options; keys ES
	//                                 does not manage keep what the player set in the RetroArch menu
	//   retroarch-core-options.cfg    core options
	//   <system>.cfg, <system>/<rom>.cfg   optional, appended by RetroArch when they exist
	//
	// Options named "retroarch.<key>" and "retroarchcore.<key>" in the ES configuration are
	// copied as they are into retroarch.cfg and the core options.
	class RetroArchGenerator : public Generator
	{
	public:
		Command configure(const LaunchContext& ctx) override;

		// Where libretro cores are looked for, in order: $ES_LIBRETRO_CORES_DIR, the cores RetroArch
		// downloads itself, then the distribution folders (Arch, Debian/Armbian).
		static std::vector<std::filesystem::path> coreDirectories();

		// The pieces configure() is made of, public for the tests
		static void setPaths(const LaunchContext& ctx, ConfigFile& config, const std::filesystem::path& coresDir);
		static void setVideoAndAudio(const LaunchContext& ctx, ConfigFile& config);
		static void setControllers(const LaunchContext& ctx, ConfigFile& config);
		static void setSaveStates(const LaunchContext& ctx, ConfigFile& config);
		static void setAchievements(const LaunchContext& ctx, ConfigFile& config);
		static void setNetplay(const LaunchContext& ctx, ConfigFile& config);
	};
}
