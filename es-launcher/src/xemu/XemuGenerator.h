// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.xemu), Copyright (c) 2026 The Batocera Team.

#pragma once

#include "es-launcher/Generator.h"
#include "util/IniFile.h"

namespace es::launcher::xemu
{
	// Configures xemu (Xbox, x86_64 only). Registered as "xemu".
	//
	// xemu is started with -config_path ctx.paths.configFor("xemu")/xemu.toml. It needs, in the
	// BIOS folder, the MCPX boot ROM (mcpx_1.0.bin) and a flash ROM (Complex_4627.bin); the hard
	// disk image and the EEPROM live in <saves>/xbox. When <saves>/xbox/xbox_hdd.qcow2 is missing,
	// ES copies the xbox_hdd.qcow2 of the BIOS folder there.
	//
	// Options are the ones of batocera-launch ("xemu_render", "xemu_aspect"...).
	class XemuGenerator : public Generator
	{
	public:
		Command configure(const LaunchContext& ctx) override;

		// Public for the tests
		static void writeSettings(const LaunchContext& ctx, util::IniFile& toml);
	};
}
