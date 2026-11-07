// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch-dolphin, Copyright (c) 2026 The Batocera Team.

#pragma once

#include "es-launcher/Generator.h"
#include "util/IniFile.h"

#include <filesystem>

namespace es::launcher::dolphin
{
	// Configures Dolphin (GameCube and Wii). Registered as "dolphin".
	//
	// Dolphin is started with -u ctx.paths.configFor("dolphin"), its user folder: Config/ holds
	// the settings. GC/ and Wii/ (memory cards, the Wii's NAND) are links to <saves>/gamecube/GC
	// and <saves>/wii/Wii, StateSaves/ to <saves>/dolphin/StateSaves.
	//
	// Pads are read through SDL. On the Wii they are emulated Wii Remotes unless
	// "emulatedwiimotes" is off (real Wii Remotes, and GameCube pads); "controller_mode" adds an
	// extension: "cc" or "pro" (Classic Controller), "in" (Nunchuk), "side" (held sideways).
	// Other options are the ones of batocera-launch ("internal_resolution", "dolphin_aspect_ratio"...).
	class DolphinGenerator : public Generator
	{
	public:
		Command configure(const LaunchContext& ctx) override;

		static std::filesystem::path userDirectory(const LaunchContext& ctx);

		// The pieces configure() is made of, public for the tests
		static void writeSettings(const LaunchContext& ctx, util::IniFile& dolphin, util::IniFile& gfx);
		static void writeGameCubePads(const LaunchContext& ctx, util::IniFile& pads);
		static void writeWiimotes(const LaunchContext& ctx, util::IniFile& wiimotes);
		static void writeHotkeys(const LaunchContext& ctx, util::IniFile& hotkeys);
		static void writeAchievements(const LaunchContext& ctx, util::IniFile& achievements);

		static bool emulatedWiimotes(const LaunchContext& ctx);
	};
}
