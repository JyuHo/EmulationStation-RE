// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.xemu), Copyright (c) 2026 The Batocera Team.

#include "xemu/XemuGenerator.h"

#include "es-launcher/LaunchError.h"
#include "util/EmulatorCommand.h"
#include "util/Gamepad.h"

namespace es::launcher::xemu
{
	namespace fs = std::filesystem;
	using util::IniFile;
	using util::tomlString;

	ES_LAUNCHER_REGISTER_GENERATOR("xemu", XemuGenerator)

	namespace
	{
		constexpr const char* bootRom = "mcpx_1.0.bin";
		constexpr const char* flashRom = "Complex_4627.bin";
		constexpr const char* hardDisk = "xbox_hdd.qcow2";

		// TOML booleans are written bare
		std::string tomlBool(const Settings& s, std::string_view key, bool fallback)
		{
			return s.getBool(key, fallback) ? "true" : "false";
		}
	}

	void XemuGenerator::writeSettings(const LaunchContext& ctx, IniFile& toml)
	{
		const Settings& s = ctx.settings;
		fs::path saves = ctx.paths.saves / "xbox";

		toml.set("general", "show_welcome", "false");
		toml.set("general", "skip_boot_anim", tomlBool(s, "xemu_bootanim", false));
		toml.set("general", "screenshot_dir", tomlString(ctx.paths.screenshots.string()));

		toml.set("sys", "mem_limit", tomlString(s.getString("xemu_memory", "64")));
		toml.set("sys.files", "bootrom_path", tomlString((ctx.paths.bios / bootRom).string()));
		toml.set("sys.files", "flashrom_path", tomlString((ctx.paths.bios / flashRom).string()));
		toml.set("sys.files", "hdd_path", tomlString((saves / hardDisk).string()));
		toml.set("sys.files", "eeprom_path", tomlString((saves / "xemu_eeprom.bin").string()));
		toml.set("sys.files", "dvd_path", tomlString(ctx.game.rom.string()));

		toml.set("audio", "use_dsp", tomlBool(s, "xemu_use_dsp", false));

		toml.set("display", "renderer", tomlString(s.getString("xemu_api", "VULKAN")));
		toml.set("display.quality", "surface_scale", s.getString("xemu_render", "1"));
		toml.set("display.window", "fullscreen_on_startup", "true");
		if (ctx.screenWidth > 0 && ctx.screenHeight > 0)
			toml.set("display.window", "startup_size", tomlString(std::to_string(ctx.screenWidth) + "x" + std::to_string(ctx.screenHeight)));
		toml.set("display.window", "vsync", tomlBool(s, "xemu_vsync", true));
		toml.set("display.ui", "show_menubar", "false");
		toml.set("display.ui", "fit", tomlString(s.getString("xemu_scaling", "scale")));
		toml.set("display.ui", "aspect_ratio", tomlString(s.getString("xemu_aspect", "auto")));

		// pads by SDL GUID, one per port
		for (int port = 1; port <= 4; ++port)
			toml.remove("input.bindings", "port" + std::to_string(port));
		int port = 0;
		for (const Controller& pad : ctx.controllers)
		{
			if (++port > 4)
				break;
			toml.set("input.bindings", "port" + std::to_string(port), tomlString(pad.guid));
		}

		// https://github.com/xemu-project/xemu/blob/master/config_spec.yml
		std::string network = s.getString("xemu_networktype");
		toml.set("net", "enable", network.empty() ? "false" : "true");
		if (!network.empty())
			toml.set("net", "backend", tomlString(network));
		if (s.isSet("xemu_udpremote"))
			toml.set("net.udp", "remote_addr", tomlString(s.getString("xemu_udpremote")));
		if (s.isSet("xemu_udpbind"))
			toml.set("net.udp", "bind_addr", tomlString(s.getString("xemu_udpbind")));
	}

	Command XemuGenerator::configure(const LaunchContext& ctx)
	{
		if (ctx.architecture != Architecture::X86_64)
			throw LaunchError(ErrorCode::Generic, "xemu only runs on x86_64 computers.");

		std::error_code ec;
		for (const char* file : { bootRom, flashRom })
			if (!fs::is_regular_file(ctx.paths.bios / file, ec))
				throw LaunchError(ErrorCode::Generic, std::string("The Xbox BIOS file ") + file + " is missing from " + ctx.paths.bios.string() + ".");

		fs::path home = ctx.paths.configFor("xemu");
		fs::path saves = ctx.paths.saves / "xbox";

		auto program = util::emulatorCommand(ctx, "xemu", "xemu", {
			home, saves, ctx.paths.bios, ctx.paths.screenshots, ctx.game.rom.parent_path() });
		if (!program)
			throw LaunchError(ErrorCode::EmulatorMissing, "xemu is not installed: install it from the emulator catalog or with your package manager.");

		// the hard disk: the player's, else a copy of the one in the BIOS folder
		fs::path disk = saves / hardDisk;
		if (!fs::is_regular_file(disk, ec))
		{
			fs::path blank = ctx.paths.bios / hardDisk;
			if (!fs::is_regular_file(blank, ec))
				throw LaunchError(ErrorCode::Generic, std::string("The Xbox hard disk image ") + hardDisk + " is missing: put one in " + ctx.paths.bios.string() + ".");

			fs::create_directories(saves, ec);
			if (!fs::copy_file(blank, disk, ec))
				throw LaunchError(ErrorCode::InvalidConfiguration, "Cannot copy the Xbox hard disk image to " + disk.string() + ": " + ec.message());
		}

		fs::path configPath = home / "xemu.toml";
		IniFile toml = IniFile::load(configPath);
		writeSettings(ctx, toml);
		try
		{
			fs::create_directories(ctx.paths.screenshots);
			toml.save(configPath);
		}
		catch (const fs::filesystem_error& e)
		{
			throw LaunchError(ErrorCode::InvalidConfiguration, std::string("Cannot write the xemu configuration: ") + e.what());
		}

		Command cmd;
		cmd.argv = *program;
		cmd.argv.insert(cmd.argv.end(), { "-config_path", configPath.string() });

		cmd.env["XDG_CONFIG_HOME"] = home.string();
		cmd.env["LC_NUMERIC"] = "C";
		cmd.env["SDL_GAMECONTROLLERCONFIG"] = util::gameControllerConfig(ctx.controllers);
		cmd.closeOnHotkey = true;   // no pad hotkeys of its own
		util::forwardEnvironment(cmd);
		return cmd;
	}
}
