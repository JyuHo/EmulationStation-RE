// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.xenia_edge), Copyright (c) 2026 The Batocera Team.

#include "xenia/XeniaGenerator.h"

#include "es-launcher/LaunchError.h"
#include "util/EmulatorCommand.h"
#include "util/Gamepad.h"

#include <fstream>

namespace es::launcher::xenia
{
	namespace fs = std::filesystem;
	using util::IniFile;
	using util::tomlString;

	ES_LAUNCHER_REGISTER_GENERATOR("xenia-edge", XeniaGenerator)

	namespace
	{
		std::string tomlBool(bool value) { return value ? "true" : "false"; }
	}

	fs::path XeniaGenerator::dataDirectory(const LaunchContext& ctx)
	{
		return ctx.paths.configFor("xenia-edge") / "Xenia";
	}

	fs::path XeniaGenerator::resolvePlaylist(const fs::path& rom)
	{
		if (rom.extension() != ".xbox360")
			return rom;

		std::ifstream in(rom);
		std::string line;
		std::getline(in, line);
		while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
			line.pop_back();
		while (!line.empty() && (line.front() == '/' || line.front() == ' '))
			line.erase(line.begin());

		std::error_code ec;
		fs::path target = rom.parent_path() / line;
		if (line.empty() || !fs::exists(target, ec))
			throw LaunchError(ErrorCode::Generic, "The game named in " + rom.filename().string() + " was not found (" + target.string() + ").");
		return target;
	}

	void XeniaGenerator::writeSettings(const LaunchContext& ctx, IniFile& toml)
	{
		const Settings& s = ctx.settings;
		std::string refresh = s.getString("xenia_edge_guest_refresh_rate", "60hz");
		std::string scale = s.getString("xenia_edge_resolution_scale", "1");

		toml.set("APU", "apu", tomlString(s.getString("xenia_edge_apu", "alsa")));

		toml.set("CPU", "break_on_unimplemented_instructions", "false");
		toml.set("CPU", "disable_context_promotion", tomlBool(s.getBool("xenia_edge_disable_context_promotion", false)));

		toml.set("Content", "license_mask", s.getString("xenia_edge_license", "1"));

		toml.set("Console", "internal_display_resolution", s.getString("xenia_edge_resolution", "8"));
		toml.set("Console", "user_country", s.getString("xenia_edge_country", "103"));
		toml.set("Console", "user_language", s.getString("xenia_edge_language", "1"));
		toml.set("Console", "widescreen", tomlBool(s.getBool("xenia_edge_widescreen", true)));
		toml.set("Console", "use_50Hz_mode", tomlBool(refresh == "50hz"));

		toml.set("Display", "fullscreen", "true");
		toml.set("Display", "postprocess_scaling_and_sharpening", tomlString(s.getString("xenia_edge_postprocess_scaling_and_sharpening", "bilinear")));
		toml.set("Display", "postprocess_antialiasing", tomlString(s.getString("xenia_edge_postprocess_antialiasing", "none")));
		toml.set("Display", "postprocess_ffx_cas_additional_sharpness", s.getString("xenia_edge_postprocess_ffx_cas_additional_sharpness", "0.0"));
		toml.set("Display", "postprocess_ffx_fsr_sharpness_reduction", s.getString("xenia_edge_postprocess_ffx_fsr_sharpness_reduction", "0.2"));
		toml.set("Display", "present_letterbox", "true");

		toml.set("General", "discord", "false");
		toml.set("General", "apply_patches", tomlBool(s.getBool("xenia_edge_patches", false)));

		toml.set("GPU", "gpu", tomlString("vulkan"));
		toml.set("GPU", "framerate_limit", s.getString("xenia_edge_vsync_fps", "0"));
		toml.set("GPU", "render_target_path", tomlString(s.getString("xenia_edge_render_target_path", "performance")));
		toml.set("GPU", "occlusion_query", tomlString(s.getString("xenia_edge_occlusion_query", "fast")));
		toml.set("GPU", "precise_interpolation", tomlBool(s.getBool("xenia_edge_precise_interpolation", true)));
		toml.set("GPU", "async_shader_compilation", tomlBool(s.getBool("xenia_edge_async_shader_compilation", true)));
		toml.set("GPU", "draw_resolution_scale_x", scale);
		toml.set("GPU", "draw_resolution_scale_y", scale);
		toml.set("GPU", "guest_display_refresh_cap", tomlBool(refresh != "uncapped"));
		toml.set("GPU.Debug", "texture_cache_memory_limit_hard", s.getString("xenia_edge_limit_hard", "768"));
		toml.set("GPU.Debug", "texture_cache_memory_limit_render_to_texture", s.getString("xenia_edge_limit_render_to_texture", "24"));
		toml.set("GPU.Debug", "texture_cache_memory_limit_soft", s.getString("xenia_edge_limit_soft", "384"));
		toml.set("GPU.Debug", "texture_cache_memory_limit_soft_lifetime", s.getString("xenia_edge_limit_soft_lifetime", "30"));

		toml.set("HID", "hid", tomlString("sdl"));
		toml.set("HID", "left_stick_deadzone_percentage", s.getString("xenia_edge_deadzone_left", "0.0"));
		toml.set("HID", "right_stick_deadzone_percentage", s.getString("xenia_edge_deadzone_right", "0.0"));
		toml.set("HID", "vibration", tomlBool(s.getBool("xenia_edge_vibration", true)));

		toml.set("Linux", "use_gamemode", "false");
		toml.set("Linux", "use_mangohud", "false");
		toml.set("Memory", "protect_zero", "false");

		toml.set("Storage", "storage_root", tomlString(dataDirectory(ctx).string()));
		toml.set("Storage", "content_root", tomlString((ctx.paths.saves / "xbox360").string()));
		toml.set("Storage", "cache_root", tomlString((ctx.paths.cache / "xenia-edge").string()));
		toml.set("Storage", "mount_scratch", "true");
		toml.set("Storage", "mount_cache", tomlBool(s.getBool("xenia_edge_cache", true)));

		toml.set("UI", "guide_button", "false");
		toml.set("UI", "show_achievement_notification", tomlBool(s.getBool("xenia_edge_achievement", false)));

		toml.set("Vulkan", "vulkan_sparse_shared_memory", "false");
		toml.set("Vulkan", "vulkan_allow_present_mode_immediate", tomlBool(!s.getBool("xenia_edge_vsync", true)));
	}

	Command XeniaGenerator::configure(const LaunchContext& ctx)
	{
		if (ctx.architecture != Architecture::X86_64)
			throw LaunchError(ErrorCode::Generic, "Xenia only runs on x86_64 computers.");

		fs::path home = ctx.paths.configFor("xenia-edge");
		fs::path saves = ctx.paths.saves / "xbox360";
		fs::path rom = resolvePlaylist(ctx.game.rom);

		auto program = util::emulatorCommand(ctx, "xenia-edge", { "xenia_edge", "xenia-edge" }, {
			home, saves, ctx.paths.cache, ctx.game.rom.parent_path() });
		if (!program)
			throw LaunchError(ErrorCode::EmulatorMissing, "Xenia Edge is not installed: install it from the emulator catalog.");

		fs::path configPath = dataDirectory(ctx) / "xenia-edge.config.toml";
		IniFile toml = IniFile::load(configPath);
		writeSettings(ctx, toml);
		try
		{
			for (const fs::path& dir : { saves, ctx.paths.cache / "xenia-edge", dataDirectory(ctx) / "patches" })
				fs::create_directories(dir);
			toml.save(configPath);
		}
		catch (const fs::filesystem_error& e)
		{
			throw LaunchError(ErrorCode::InvalidConfiguration, std::string("Cannot write the Xenia configuration: ") + e.what());
		}

		Command cmd;
		cmd.argv = *program;
		cmd.argv.push_back(rom.string());

		cmd.env["XDG_DATA_HOME"] = home.string();
		cmd.env["SDL_GAMECONTROLLERCONFIG"] = util::gameControllerConfig(ctx.controllers);
		cmd.closeOnHotkey = true;   // no pad hotkeys of its own
		util::forwardEnvironment(cmd);
		return cmd;
	}
}
