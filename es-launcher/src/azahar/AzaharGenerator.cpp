// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.azahar), Copyright (c) 2026 The Batocera Team.

#include "azahar/AzaharGenerator.h"

#include "es-launcher/LaunchError.h"
#include "util/EmulatorCommand.h"
#include "util/Gamepad.h"

#include <cstdlib>

namespace es::launcher::azahar
{
	namespace fs = std::filesystem;
	using util::IniFile;

	ES_LAUNCHER_REGISTER_GENERATOR("azahar", AzaharGenerator)

	namespace
	{
		// Azahar button -> ES input
		constexpr std::pair<const char*, const char*> buttons[] = {
			{ "button_a", "a" }, { "button_b", "b" }, { "button_x", "x" }, { "button_y", "y" },
			{ "button_up", "up" }, { "button_down", "down" }, { "button_left", "left" }, { "button_right", "right" },
			{ "button_l", "pageup" }, { "button_r", "pagedown" }, { "button_start", "start" }, { "button_select", "select" },
			{ "button_zl", "l2" }, { "button_zr", "r2" }, { "button_home", "hotkey" },
		};

		// 3DS regions by locale, for the system region
		constexpr std::pair<const char*, int> regions[] = {
			{ "ja_JP", 0 }, { "en_US", 1 }, { "de_DE", 2 }, { "es_ES", 2 }, { "fr_FR", 2 }, { "it_IT", 2 },
			{ "hu_HU", 2 }, { "pt_PT", 2 }, { "ru_RU", 2 }, { "en_AU", 3 }, { "zh_CN", 4 }, { "ko_KR", 5 }, { "zh_TW", 6 },
		};

		int region()
		{
			const char* lang = std::getenv("LANG");
			std::string_view code = lang != nullptr ? std::string_view(lang).substr(0, 5) : std::string_view();
			for (const auto& [locale, value] : regions)
				if (code == locale)
					return value;
			return -1;   // automatic
		}

		const char* hatDirection(int value)
		{
			switch (value)
			{
			case 1: return "up";
			case 2: return "right";
			case 4: return "down";
			case 8: return "left";
			default: return "unknown";
			}
		}

		// Azahar's SDL binding of one ES input, by joystick number and GUID
		std::string buttonBinding(const Controller& pad, std::string_view input)
		{
			const InputBinding* binding = pad.find(input);
			if (binding == nullptr)
				return {};

			std::string guid = "guid:" + pad.guid + ",port:0,engine:sdl";
			std::string id = std::to_string(binding->id);
			switch (binding->type)
			{
			case InputBinding::Type::Button:
				return "button:" + id + "," + guid;
			case InputBinding::Type::Hat:
				return "hat:" + id + ",direction:" + hatDirection(binding->value) + "," + guid;
			case InputBinding::Type::Axis:
				return std::string("axis:") + id + ",direction:" + (binding->value < 0 ? "-" : "+") + ",threshold:0.5," + guid;
			case InputBinding::Type::Key:
				break;
			}
			return {};
		}

		std::string stickBinding(const Controller& pad, std::string_view stick)
		{
			const InputBinding* x = pad.find(std::string(stick) + "left");
			const InputBinding* y = pad.find(std::string(stick) + "up");
			if (x == nullptr || y == nullptr || x->type != InputBinding::Type::Axis || y->type != InputBinding::Type::Axis)
				return {};
			return "axis_x:" + std::to_string(x->id) + ",axis_y:" + std::to_string(y->id) + ",guid:" + pad.guid + ",port:0,engine:sdl";
		}

		// Qt settings: a value, and \default = false so Azahar does not reset it
		void setQt(IniFile& ini, std::string_view section, const std::string& key, std::string_view value)
		{
			ini.set(section, key, value);
			ini.set(section, key + "\\default", "false");
		}

		const char* boolString(bool value) { return value ? "true" : "false"; }
	}

	fs::path AzaharGenerator::settingsPath(const LaunchContext& ctx)
	{
		return ctx.paths.configFor("azahar") / "azahar-emu" / "qt-config.ini";
	}

	void AzaharGenerator::writeSettings(const LaunchContext& ctx, IniFile& ini)
	{
		const Settings& s = ctx.settings;

		// [Layout]: "<layout>-<swap>", the default layout without swapping
		std::string layout = s.getString("azahar_screen_layout", "0-false");
		size_t dash = layout.find('-');
		setQt(ini, "Layout", "layout_option", layout.substr(0, dash));
		setQt(ini, "Layout", "swap_screen", dash == std::string::npos ? "false" : layout.substr(dash + 1));
		setQt(ini, "Layout", "large_screen_proportion", s.getString("azahar_large_screen_proportion", "4"));

		// [System]
		setQt(ini, "System", "is_new_3ds", boolString(s.getBool("azahar_is_new_3ds", false)));
		setQt(ini, "System", "region_value", std::to_string(region()));

		// [Core]
		setQt(ini, "Core", "cpu_clock_percentage", s.getString("azahar_cpu_clock", "100"));

		// [UI]
		setQt(ini, "UI", "fullscreen", "true");
		setQt(ini, "UI", "display_titlebar", "false");
		setQt(ini, "UI", "first_start", "false");
		setQt(ini, "UI", "hide_mouse", "true");
		setQt(ini, "UI", "confirmClose", "false");
		setQt(ini, "UI", "confirm_before_closing", "false");
		setQt(ini, "UI", "enable_discord_presence", "false");
		setQt(ini, "UI", "callout_flags", "1");
		setQt(ini, "UI", "Paths\\screenshot_path", ctx.paths.screenshots.string());

		// [Miscellaneous]
		setQt(ini, "Miscellaneous", "check_for_update_on_start", "false");

		// [Renderer]: 0 software, 1 OpenGL, 2 Vulkan
		setQt(ini, "Renderer", "graphics_api", s.getString("azahar_graphics_api", "1"));
		setQt(ini, "Renderer", "use_hw_shader", boolString(s.getBool("azahar_use_hw_shader", false)));
		setQt(ini, "Renderer", "shaders_accurate_mul", boolString(s.getBool("azahar_accurate_multiplication", false)));
		setQt(ini, "Renderer", "use_shader_jit", boolString(s.getBool("azahar_use_shader_jit", true)));
		setQt(ini, "Renderer", "async_shader_compilation", boolString(s.getBool("azahar_async_shader_compilation", false)));
		setQt(ini, "Renderer", "async_presentation", boolString(s.getBool("azahar_async_presentation", false)));
		setQt(ini, "Renderer", "use_vsync", boolString(s.getBool("azahar_use_vsync", true)));
		setQt(ini, "Renderer", "resolution_factor", s.getString("azahar_resolution_factor", "1"));
		setQt(ini, "Renderer", "texture_filter", s.getString("azahar_texture_filter", "0"));
		setQt(ini, "Renderer", "use_disk_shader_cache", boolString(s.getBool("azahar_use_disk_shader_cache", true)));

		// [Audio]
		setQt(ini, "Audio", "enable_audio_stretching", boolString(s.getBool("azahar_audio_stretching", true)));

		// [Utility]: "<anything>-normal" loads custom textures as they come, else preloads them
		std::string textures = s.getString("azahar_custom_textures", "0");
		bool custom = textures != "0";
		bool normal = textures.ends_with("-normal");
		setQt(ini, "Utility", "custom_textures", boolString(custom));
		setQt(ini, "Utility", "async_custom_loading", boolString(custom && normal));
		setQt(ini, "Utility", "preload_textures", boolString(custom && !normal));

		// [Controls]: the profile Azahar creates on its first start
		if (!ini.get("Controls", "profiles\\size"))
		{
			setQt(ini, "Controls", "profile", "0");
			setQt(ini, "Controls", "profiles\\1\\name", "default");
			setQt(ini, "Controls", "profiles\\size", "1");
		}

		if (const Controller* pad = util::firstPlayer(ctx))
		{
			for (const auto& [option, input] : buttons)
				setQt(ini, "Controls", std::string("profiles\\1\\") + option, "\"" + buttonBinding(*pad, input) + "\"");
			setQt(ini, "Controls", "profiles\\1\\circle_pad", "\"" + stickBinding(*pad, "joystick1") + "\"");
			setQt(ini, "Controls", "profiles\\1\\c_stick", "\"" + stickBinding(*pad, "joystick2") + "\"");
		}
	}

	Command AzaharGenerator::configure(const LaunchContext& ctx)
	{
		fs::path home = ctx.paths.configFor("azahar");
		fs::path saves = ctx.paths.saves / "3ds";

		auto program = util::emulatorCommand(ctx, "azahar", { "azahar", "azahar-qt" }, {
			home, saves, ctx.paths.screenshots, ctx.paths.cache, ctx.game.rom.parent_path() });
		if (!program)
			throw LaunchError(ErrorCode::EmulatorMissing, "Azahar is not installed: install it from the emulator catalog or with your package manager.");

		IniFile ini = IniFile::load(settingsPath(ctx));
		writeSettings(ctx, ini);
		try
		{
			for (const fs::path& dir : { saves, ctx.paths.screenshots })
				fs::create_directories(dir);
			ini.save(settingsPath(ctx));
		}
		catch (const fs::filesystem_error& e)
		{
			throw LaunchError(ErrorCode::InvalidConfiguration, std::string("Cannot write the Azahar configuration: ") + e.what());
		}

		Command cmd;
		cmd.argv = *program;
		cmd.argv.push_back(ctx.game.rom.string());

		cmd.env["XDG_CONFIG_HOME"] = home.string();
		cmd.env["XDG_DATA_HOME"] = saves.string();
		cmd.env["XDG_CACHE_HOME"] = ctx.paths.cache.string();
		cmd.closeOnHotkey = true;   // no pad hotkeys of its own
		util::forwardEnvironment(cmd);
		return cmd;
	}
}
