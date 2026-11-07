// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch_libretro.emulator), Copyright (c) 2026 The Batocera Team.

#include "retroarch/RetroArchGenerator.h"

#include "es-launcher/LaunchError.h"
#include "retroarch/Cores.h"
#include "util/EmulatorCommand.h"
#include "util/Files.h"

#include <algorithm>
#include <array>
#include <cstdlib>

namespace es::launcher::retroarch
{
	namespace fs = std::filesystem;

	ES_LAUNCHER_REGISTER_GENERATOR("libretro", RetroArchGenerator)

	namespace
	{
		// Same order as aspect_ratios[] in RetroArch's gfx/video_driver.c: the index is what
		// aspect_ratio_index expects.
		constexpr std::array ratios = {
			"4/3", "16/9", "16/10", "16/15", "21/9", "1/1", "2/1", "3/2", "3/4", "4/1", "9/16", "5/4", "6/5",
			"7/9", "8/3", "8/7", "19/12", "19/14", "30/17", "32/9", "config", "squarepixel", "core", "custom", "full",
		};
		constexpr int coreRatioIndex = 22;

		// ES input -> RetroArch hotkey, pressed together with the hotkey button of player 1
		constexpr std::pair<const char*, const char*> hotkeys[] = {
			{ "start", "exit_emulator" },
			{ "b", "menu_toggle" },
			{ "a", "reset" },
			{ "x", "load_state" },
			{ "y", "save_state" },
			{ "up", "state_slot_increase" },
			{ "down", "state_slot_decrease" },
			{ "left", "rewind" },
			{ "right", "hold_fast_forward" },
			{ "pageup", "screenshot" },
			{ "pagedown", "ai_service" },
			{ "l2", "shader_prev" },
			{ "r2", "shader_next" },
		};

		// ES joystick input -> RetroArch analog stick axis
		constexpr std::pair<const char*, const char*> sticks[] = {
			{ "joystick1up", "l_y" },
			{ "joystick1left", "l_x" },
			{ "joystick2up", "r_y" },
			{ "joystick2left", "r_x" },
		};

		const char* typeSuffix(const InputBinding& input)
		{
			return input.type == InputBinding::Type::Axis ? "axis" : "btn";
		}

		// The value RetroArch expects for a pad input; nothing for keyboard keys, whose SDL
		// codes mean nothing to RetroArch
		std::optional<std::string> inputValue(const InputBinding& input)
		{
			switch (input.type)
			{
			case InputBinding::Type::Button:
				return std::to_string(input.id);
			case InputBinding::Type::Axis:
				return (input.value < 0 ? "-" : "+") + std::to_string(input.id);
			case InputBinding::Type::Hat:
				switch (input.value)
				{
				case 1: return "h" + std::to_string(input.id) + "up";
				case 2: return "h" + std::to_string(input.id) + "right";
				case 4: return "h" + std::to_string(input.id) + "down";
				case 8: return "h" + std::to_string(input.id) + "left";
				default: return std::nullopt;
				}
			default:
				return std::nullopt;
			}
		}

		void setBoolFromSetting(const LaunchContext& ctx, ConfigFile& config, std::string_view key, std::string_view setting, bool fallback)
		{
			config.set(key, ctx.settings.getBool(setting, fallback));
		}

		int netplayPort(const LaunchContext& ctx, const Netplay& netplay)
		{
			// the host listens on the port of its options, a client joins the port it was given
			return netplay.mode == Netplay::Mode::Host ? ctx.settings.getInt("netplay.port", netplay.port) : netplay.port;
		}

		std::string defaultVideoDriver(Architecture arch)
		{
			// desktop GL 3.3 core on PC, GLES through the "gl" driver on the Raspberry Pi
			return arch == Architecture::X86_64 ? "glcore" : "gl";
		}
	}

	std::vector<fs::path> RetroArchGenerator::coreDirectories(const fs::path& catalogCores)
	{
		std::vector<fs::path> dirs = util::pathsFromEnv("ES_LIBRETRO_CORES_DIR");

		if (!catalogCores.empty())
			dirs.push_back(catalogCores);

		fs::path configHome;
		if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg != nullptr && xdg[0] == '/')
			configHome = xdg;
		else if (const char* home = std::getenv("HOME"); home != nullptr)
			configHome = fs::path(home) / ".config";

		if (!configHome.empty())
			dirs.push_back(configHome / "retroarch" / "cores");

		for (const char* dir : { "/usr/lib/libretro", "/usr/lib64/libretro", "/usr/local/lib/libretro",
			"/usr/lib/x86_64-linux-gnu/libretro", "/usr/lib/aarch64-linux-gnu/libretro" })
			dirs.emplace_back(dir);

		return dirs;
	}

	void RetroArchGenerator::setPaths(const LaunchContext& ctx, ConfigFile& config, const fs::path& coresDir)
	{
		fs::path configDir = ctx.paths.configFor("retroarch");
		fs::path saves = ctx.paths.saves / ctx.game.system;
		fs::path cache = ctx.paths.cache / "retroarch";

		for (const fs::path& dir : { saves, ctx.paths.screenshots, cache })
		{
			std::error_code ec;
			if (!dir.empty())
				fs::create_directories(dir, ec);
		}

		config.set("core_options_path", (configDir / "retroarch-core-options.cfg").string());
		config.set("system_directory", ctx.paths.bios.string());
		config.set("savefile_directory", saves.string());
		config.set("savestate_directory", saves.string());
		config.set("screenshot_directory", ctx.paths.screenshots.string());
		config.set("cache_directory", cache.string());
		config.set("libretro_directory", coresDir.string());

		// The .info files of the emulator catalog cover all the cores it installs
		std::error_code ec;
		if (!ctx.paths.cores.empty() && fs::is_directory(ctx.paths.cores / "info", ec))
			config.set("libretro_info_path", (ctx.paths.cores / "info").string());
		else if (fs::is_directory("/usr/share/libretro/info", ec))
			config.set("libretro_info_path", "/usr/share/libretro/info");

		// saves land in saves/<system>/, never in a sub folder named after the core
		config.set("sort_savefiles_enable", false);
		config.set("sort_savestates_enable", false);
		config.set("sort_savefiles_by_content_enable", false);
		config.set("sort_savestates_by_content_enable", false);
	}

	void RetroArchGenerator::setVideoAndAudio(const LaunchContext& ctx, ConfigFile& config)
	{
		const Settings& s = ctx.settings;

		config.set("video_driver", s.getString("gfxbackend", defaultVideoDriver(ctx.architecture)));
		config.set("video_fullscreen", true);
		config.set("video_fullscreen_x", 0);   // 0: the desktop resolution
		config.set("video_fullscreen_y", 0);
		config.set("video_viewport_bias_x", "0.500000");
		config.set("video_viewport_bias_y", "0.500000");
		config.set("video_black_frame_insertion", false);

		setBoolFromSetting(ctx, config, "video_smooth", "smooth", false);
		setBoolFromSetting(ctx, config, "video_scale_integer", "integerscale", false);
		setBoolFromSetting(ctx, config, "video_threaded", "video_threaded", false);
		setBoolFromSetting(ctx, config, "vrr_runloop_enable", "vrr_runloop_enable", false);
		setBoolFromSetting(ctx, config, "video_frame_delay_auto", "video_frame_delay_auto", false);

		if (auto ratio = s.get("ratio"))
		{
			auto it = std::ranges::find(ratios, *ratio);
			int index = it == ratios.end() ? coreRatioIndex : static_cast<int>(it - ratios.begin());
			config.set("video_aspect_ratio_auto", false);
			config.set("aspect_ratio_index", index);
		}
		else
			config.remove("aspect_ratio_index");

		// Rewind and run-ahead (latency reduction)
		config.set("rewind_enable", s.getBool("rewind", false));

		int runAhead = s.getInt("runahead", 0);
		bool preemptive = runAhead > 0 && s.getBool("preemptiveframes", false);
		config.set("run_ahead_enabled", runAhead > 0 && !preemptive);
		config.set("preemptive_frames_enable", preemptive);
		config.set("run_ahead_frames", runAhead);
		config.set("run_ahead_secondary_instance", runAhead > 0 && s.getBool("secondinstance", false));

		if (auto driver = s.get("audio_driver"))
			config.set("audio_driver", *driver);
		if (auto latency = s.get("audio_latency"))
			config.set("audio_latency", *latency);

		// behave like the other emulators: one press quits, no popups, no pause when ES has focus
		config.set("quit_press_twice", false);
		config.set("pause_nonactive", false);
		config.set("menu_show_load_content_animation", false);
		config.set("menu_show_restart_retroarch", false);
		config.set("load_dummy_on_core_shutdown", false);
		config.set("builtin_imageviewer_enable", false);
		config.set("video_font_enable", true);
		config.set("notification_show_remap_load", false);

		// small screens (handhelds): no menu widgets
		bool smallScreen = ctx.screenWidth > 0 && ctx.screenHeight > 0 && (ctx.screenWidth < 480 || ctx.screenHeight < 480);
		config.set("menu_enable_widgets", !smallScreen);
	}

	void RetroArchGenerator::setControllers(const LaunchContext& ctx, ConfigFile& config)
	{
		const Core& core = coreProfile(ctx.core);

		// Forget the pads of the previous launch; RetroArch keeps its keyboard defaults
		config.removeStartingWith("input_player");
		config.removeStartingWith("input_libretro_device_p");
		config.remove("input_enable_hotkey_btn");
		config.remove("input_enable_hotkey_axis");
		for (const auto& [es, action] : hotkeys)
		{
			config.remove(std::string("input_") + action + "_btn");
			config.remove(std::string("input_") + action + "_axis");
		}

		config.set("input_joypad_driver", ctx.settings.getString("input_joypad_driver", "udev"));
		config.set("input_max_users", 16);
		config.set("input_autodetect_enable", false);   // ES already knows the mappings

		bool fightstick = ctx.settings.getString("altlayout") == "fightstick";

		for (const Controller& pad : ctx.controllers)
		{
			std::string player = "input_player" + std::to_string(pad.player) + "_";

			ButtonMap buttons = {
				{ "a", "a" }, { "b", "b" }, { "x", "x" }, { "y", "y" },
				{ "pageup", "l" }, { "pagedown", "r" }, { "l2", "l2" }, { "r2", "r2" },
				{ "l3", "l3" }, { "r3", "r3" }, { "start", "start" }, { "select", "select" },
			};

			// X Y L1 L2 / A B R1 R2  ->  X Y R1 L1 / A B R2 L2
			if (fightstick)
			{
				for (auto& [es, ra] : buttons)
				{
					if (es == "pageup") ra = "l2";
					else if (es == "pagedown") ra = "l";
					else if (es == "l2") ra = "r2";
					else if (es == "r2") ra = "r";
				}
			}

			core.adjustButtons(ctx, pad, buttons);

			for (const auto& [es, ra] : buttons)
			{
				const InputBinding* input = pad.find(es);
				if (input == nullptr)
					continue;
				if (auto value = inputValue(*input))
					config.set(player + ra + "_" + typeSuffix(*input), *value);
			}

			for (const char* direction : { "up", "down", "left", "right" })
			{
				const InputBinding* input = pad.find(direction);
				if (input == nullptr)
					continue;
				if (auto value = inputValue(*input))
					config.set(player + direction + "_" + typeSuffix(*input), *value);
			}

			for (const auto& [es, axis] : sticks)
			{
				const InputBinding* input = pad.find(es);
				if (input == nullptr || input->type != InputBinding::Type::Axis)
					continue;

				// ES records "up" and "left": the opposite direction is the other sign
				std::string id = std::to_string(input->id);
				bool negative = input->value < 0;
				config.set(player + axis + "_minus_axis", (negative ? "-" : "+") + id);
				config.set(player + axis + "_plus_axis", (negative ? "+" : "-") + id);
			}

			config.set(player + "joypad_index", pad.deviceIndex);
			config.set(player + "analog_dpad_mode", core.analogDpadMode(pad));
		}

		// Hotkeys on player 1's pad: hold the hotkey button, then press another one
		if (!ctx.controllers.empty())
		{
			const Controller& pad = ctx.controllers.front();
			const InputBinding* hotkey = pad.find("hotkey");
			if (hotkey != nullptr && hotkey->type == InputBinding::Type::Button)
			{
				config.set("input_enable_hotkey_btn", hotkey->id);

				for (const auto& [es, action] : hotkeys)
				{
					const InputBinding* input = pad.find(es);
					if (input == nullptr)
						continue;
					if (auto value = inputValue(*input))
						config.set(std::string("input_") + action + "_" + typeSuffix(*input), *value);
				}
			}
		}

		// devices other than the RetroPad (multitap, mouse...) asked by the core
		for (int p = 1; p <= 4; ++p)
		{
			if (auto device = core.deviceType(ctx, p))
				config.set("input_libretro_device_p" + std::to_string(p), *device);
		}
	}

	void RetroArchGenerator::setSaveStates(const LaunchContext& ctx, ConfigFile& config)
	{
		const Settings& s = ctx.settings;

		bool autoSave = s.getBool("autosave", false);
		config.set("savestate_auto_save", autoSave);
		config.set("savestate_auto_load", autoSave || ctx.saveState.autoSave);

		bool incremental = s.getBool("incrementalsavestates", true);
		config.set("savestate_auto_index", incremental);
		config.set("savestate_max_keep", incremental ? 0 : 50);

		config.set("state_slot", ctx.saveState.slot.value_or(0));
	}

	void RetroArchGenerator::setAchievements(const LaunchContext& ctx, ConfigFile& config)
	{
		const Settings& s = ctx.settings;

		bool enabled = s.getBool("retroachievements", false) && !ctx.netplay;
		config.set("cheevos_enable", enabled);

		if (!enabled)
			return;

		config.set("cheevos_username", s.getString("retroachievements.username"));
		if (auto token = s.get("retroachievements.token"))
		{
			config.set("cheevos_token", *token);
			config.set("cheevos_password", "");
		}
		else
			config.set("cheevos_password", s.getString("retroachievements.password"));

		setBoolFromSetting(ctx, config, "cheevos_hardcore_mode_enable", "retroachievements.hardcore", false);
		setBoolFromSetting(ctx, config, "cheevos_leaderboards_enable", "retroachievements.leaderboards", false);
		setBoolFromSetting(ctx, config, "cheevos_verbose_enable", "retroachievements.verbose", false);
		setBoolFromSetting(ctx, config, "cheevos_auto_screenshot", "retroachievements.screenshot", false);
		setBoolFromSetting(ctx, config, "cheevos_challenge_indicators", "retroachievements.challenge_indicators", false);
		setBoolFromSetting(ctx, config, "cheevos_start_active", "retroachievements.encore", false);
		setBoolFromSetting(ctx, config, "cheevos_richpresence_enable", "retroachievements.richpresence", false);
		setBoolFromSetting(ctx, config, "cheevos_test_unofficial", "retroachievements.unofficial", false);
	}

	void RetroArchGenerator::setNetplay(const LaunchContext& ctx, ConfigFile& config)
	{
		if (!ctx.netplay)
			return;

		const Netplay& netplay = *ctx.netplay;
		const Settings& s = ctx.settings;

		// hardcore achievements disable save states, which netplay needs
		config.set("cheevos_hardcore_mode_enable", false);
		config.set("netplay_ip_port", netplayPort(ctx, netplay));
		config.set("netplay_nickname", s.getString("netplay.nickname", "Player"));

		if (auto frames = s.get("netplay.frames"))
			config.set("netplay_input_latency_frames_min", *frames);

		switch (netplay.mode)
		{
		case Netplay::Mode::Host:
			config.set("netplay_start_as_spectator", false);
			config.set("netplay_password", s.getString("netplay.password"));
			config.set("netplay_spectate_password", s.getString("netplay.spectatepassword"));
			break;
		case Netplay::Mode::Client:
			config.set("netplay_start_as_spectator", false);
			config.set("netplay_ip_address", netplay.host);
			config.set("netplay_password", netplay.password);
			break;
		case Netplay::Mode::Spectator:
			config.set("netplay_start_as_spectator", true);
			config.set("netplay_ip_address", netplay.host);
			config.set("netplay_spectate_password", netplay.password);
			break;
		}

		config.set("netplay_public_announce", s.getBool("netplay_public_announce", true));

		std::string relay = s.getString("netplay.relay", "none");
		config.set("netplay_use_mitm_server", relay != "none");
		if (relay != "none")
		{
			config.set("netplay_mitm_server", relay);
			if (relay == "custom")
				config.set("netplay_custom_mitm_server", s.getString("netplay.customserver"));
		}
	}

	Command RetroArchGenerator::configure(const LaunchContext& ctx)
	{
		if (ctx.core.empty())
			throw LaunchError(ErrorCode::InvalidConfiguration, "No libretro core is selected for this game.");

		std::string library = coreLibraryName(ctx.core);
		auto core = util::findFile(library, coreDirectories(ctx.paths.cores));
		if (!core)
			throw LaunchError(ErrorCode::CoreMissing, "The libretro core \"" + ctx.core + "\" is not installed (" + library + ").");

		// The catalog installs RetroArch under the id "retroarch", whatever ctx.emulator says
		// ("libretro" in es_systems.cfg). A Flatpak RetroArch needs to reach everything below.
		auto retroarch = util::emulatorCommand(ctx, "retroarch", "retroarch", {
			ctx.paths.configFor("retroarch"), ctx.paths.saves, ctx.paths.bios, ctx.paths.screenshots,
			ctx.paths.cache, core->parent_path(), ctx.game.rom.parent_path() });
		if (!retroarch)
			throw LaunchError(ErrorCode::EmulatorMissing, "RetroArch is not installed: install it from the emulator catalog or with your package manager.");

		const Core& profile = coreProfile(ctx.core);
		fs::path configDir = ctx.paths.configFor("retroarch");
		fs::path configPath = configDir / "retroarch.cfg";
		fs::path optionsPath = configDir / "retroarch-core-options.cfg";

		// retroarch.cfg
		ConfigFile config = ConfigFile::load(configPath);
		setPaths(ctx, config, core->parent_path());
		setVideoAndAudio(ctx, config);
		setControllers(ctx, config);
		setSaveStates(ctx, config);
		setAchievements(ctx, config);
		setNetplay(ctx, config);
		profile.setConfig(ctx, config);

		for (const auto& [key, value] : ctx.settings.withPrefix("retroarch."))
			config.set(key, value);

		// core options
		ConfigFile options = ConfigFile::load(optionsPath);
		profile.setCoreOptions(ctx, options);

		for (const auto& [key, value] : ctx.settings.withPrefix("retroarchcore."))
			options.set(key, value);

		try
		{
			config.save(configPath);
			options.save(optionsPath);
		}
		catch (const fs::filesystem_error& e)
		{
			throw LaunchError(ErrorCode::InvalidConfiguration, std::string("Cannot write the RetroArch configuration: ") + e.what());
		}

		Command cmd;
		cmd.argv = *retroarch;
		cmd.argv.insert(cmd.argv.end(), { "-L", core->string(), "--config", configPath.string() });

		// the player's own per-system and per-game overrides
		std::string append;
		for (const fs::path& extra : { configDir / (ctx.game.system + ".cfg"), configDir / ctx.game.system / (util::fileName(ctx.game.rom) + ".cfg") })
		{
			std::error_code ec;
			if (fs::is_regular_file(extra, ec))
				append += (append.empty() ? "" : "|") + extra.string();
		}
		if (!append.empty())
			cmd.argv.insert(cmd.argv.end(), { "--appendconfig", append });

		if (ctx.netplay)
		{
			const Netplay& netplay = *ctx.netplay;
			if (netplay.mode == Netplay::Mode::Host)
				cmd.argv.push_back("--host");
			else
				cmd.argv.insert(cmd.argv.end(), { "--connect", netplay.host });

			cmd.argv.insert(cmd.argv.end(), { "--port", std::to_string(netplayPort(ctx, netplay)) });

			if (!netplay.session.empty())
				cmd.argv.insert(cmd.argv.end(), { "--mitm-session", netplay.session });

			cmd.argv.insert(cmd.argv.end(), { "--nick", ctx.settings.getString("netplay.nickname", "Player") });
		}

		// load a save state chosen in ES (the autosave is loaded through savestate_auto_load)
		if (ctx.saveState.slot && !ctx.saveState.file.empty() && !ctx.saveState.autoSave)
			cmd.argv.insert(cmd.argv.end(), { "-e", std::to_string(*ctx.saveState.slot) });

		cmd.argv.push_back(ctx.game.rom.string());
		return cmd;
	}
}

// the emulator is called "libretro" in es_systems.cfg; "retroarch" is accepted too
namespace
{
	using es::launcher::retroarch::RetroArchGenerator;
	const bool registeredRetroArchAlias = es::launcher::Registry::instance().add(
		"retroarch", [] { return std::unique_ptr<es::launcher::Generator>(new RetroArchGenerator()); });
}
