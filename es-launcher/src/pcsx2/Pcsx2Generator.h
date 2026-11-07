// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch-pcsx2, Copyright (c) 2026 The Batocera Team.

#pragma once

#include "es-launcher/Generator.h"
#include "util/IniFile.h"

#include <filesystem>
#include <optional>

namespace es::launcher::pcsx2
{
	// Configures PCSX2 (PlayStation 2, x86_64 only). Registered as "pcsx2".
	//
	// PCSX2 keeps everything in $XDG_CONFIG_HOME/PCSX2: ES points XDG_CONFIG_HOME at
	// ctx.paths.configFor("pcsx2"), so the settings are in <that>/PCSX2/inis/PCSX2.ini.
	// The BIOS is looked for in <bios>/ps2, then <bios>; memory cards go to <saves>/ps2.
	//
	// Options are the ones of batocera-launch ("pcsx2_gfxbackend", "pcsx2_ratio"...).
	class Pcsx2Generator : public Generator
	{
	public:
		Command configure(const LaunchContext& ctx) override;

		static std::filesystem::path dataDirectory(const LaunchContext& ctx);

		// A PS2 BIOS image in dir (a 4 to 8 MiB file), preferring `preferred` when it is one.
		static std::optional<std::filesystem::path> findBios(const std::filesystem::path& dir, const std::string& preferred = {});

		// The pieces configure() is made of, public for the tests. Throw LaunchError.
		static void writeSettings(const LaunchContext& ctx, util::IniFile& ini);
		static void writeControllers(const LaunchContext& ctx, util::IniFile& ini);
	};
}
