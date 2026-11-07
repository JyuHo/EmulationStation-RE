// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.duckstation), Copyright (c) 2026 The Batocera Team.

#include "duckstation/DuckStationGenerator.h"

#include "es-launcher/LaunchError.h"
#include "util/EmulatorCommand.h"
#include "util/Files.h"
#include "util/Gamepad.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <fstream>

namespace es::launcher::duckstation
{
	namespace fs = std::filesystem;
	using util::IniFile;
	using util::PadControl;

	ES_LAUNCHER_REGISTER_GENERATOR("duckstation", DuckStationGenerator)

	namespace
	{
		constexpr std::pair<const char*, std::array<const char*, 8>> biosLists[] = {
			{ "NTSCU", { "scph101.bin", "scph1001.bin", "scph5501.bin", "scph7001.bin", "scph7501.bin" } },
			{ "PAL", { "scph1002.bin", "scph5502.bin", "scph5552.bin", "scph7002.bin", "scph7502.bin", "scph9002.bin", "scph102a.bin", "scph102b.bin" } },
			{ "NTSCJ", { "scph100.bin", "scph1000.bin", "scph3000.bin", "scph3500.bin", "scph5500.bin", "scph7000.bin", "scph7003.bin" } },
			{ "Uni", { "psxonpsp660.bin", "ps1_rom.bin" } },
		};

		constexpr std::pair<const char*, const char*> biosKeys[] = {
			{ "NTSCU", "PathNTSCU" },
			{ "PAL", "PathPAL" },
			{ "NTSCJ", "PathNTSCJ" },
		};

		constexpr std::pair<const char*, const char*> languages[] = {
			{ "en_US", "en" }, { "de_DE", "de" }, { "fr_FR", "fr" }, { "es_ES", "es" }, { "he_IL", "he" },
			{ "it_IT", "it" }, { "ja_JP", "ja" }, { "nl_NL", "nl" }, { "pl_PL", "pl" }, { "pt_BR", "pt-br" },
			{ "pt_PT", "pt-pt" }, { "ru_RU", "ru" }, { "zh_CN", "zh-cn" },
		};

		// PlayStation pad control -> SDL gamepad control, as DuckStation names it
		constexpr std::pair<const char*, const char*> padBindings[] = {
			{ "Up", "DPadUp" }, { "Right", "DPadRight" }, { "Down", "DPadDown" }, { "Left", "DPadLeft" },
			{ "Triangle", "Y" }, { "Circle", "B" }, { "Cross", "A" }, { "Square", "X" },
			{ "Select", "Back" }, { "Start", "Start" },
			{ "L1", "LeftShoulder" }, { "R1", "RightShoulder" }, { "L2", "+LeftTrigger" }, { "R2", "+RightTrigger" },
			{ "L3", "LeftStick" }, { "R3", "RightStick" },
			{ "LLeft", "-LeftX" }, { "LRight", "+LeftX" }, { "LDown", "+LeftY" }, { "LUp", "-LeftY" },
			{ "RLeft", "-RightX" }, { "RRight", "+RightX" }, { "RDown", "+RightY" }, { "RUp", "-RightY" },
			{ "SmallMotor", "SmallMotor" }, { "LargeMotor", "LargeMotor" },
		};

		constexpr std::pair<const char*, const char*> negconBindings[] = {
			{ "A", "B" }, { "B", "Y" }, { "I", "+RightTrigger" }, { "II", "+LeftTrigger" },
			{ "L", "LeftShoulder" }, { "R", "RightShoulder" }, { "SteeringLeft", "-LeftX" }, { "SteeringRight", "+LeftX" },
		};

		// Without a pad, the keyboard keys batocera-launch uses
		constexpr std::pair<const char*, const char*> keyboardHotkeys[] = {
			{ "FastForward", "Keyboard/Tab" },
			{ "Reset", "Keyboard/F6" },
			{ "LoadSelectedSaveState", "Keyboard/F1" },
			{ "SaveSelectedSaveState", "Keyboard/F2" },
			{ "SelectPreviousSaveStateSlot", "Keyboard/F3" },
			{ "SelectNextSaveStateSlot", "Keyboard/F4" },
			{ "Screenshot", "Keyboard/F10" },
			{ "Rewind", "Keyboard/F5" },
			{ "OpenPauseMenu", "Keyboard/F7" },
			{ "ChangeDisc", "Keyboard/F8" },
			{ "PowerOff", "Keyboard/Escape" },
		};

		const char* controlName(PadControl control)
		{
			switch (control)
			{
			case PadControl::South: return "A";
			case PadControl::East: return "B";
			case PadControl::West: return "X";
			case PadControl::North: return "Y";
			case PadControl::Back: return "Back";
			case PadControl::Guide: return "Guide";
			case PadControl::Start: return "Start";
			case PadControl::LeftStick: return "LeftStick";
			case PadControl::RightStick: return "RightStick";
			case PadControl::LeftShoulder: return "LeftShoulder";
			case PadControl::RightShoulder: return "RightShoulder";
			case PadControl::DPadUp: return "DPadUp";
			case PadControl::DPadDown: return "DPadDown";
			case PadControl::DPadLeft: return "DPadLeft";
			case PadControl::DPadRight: return "DPadRight";
			case PadControl::LeftTrigger: return "+LeftTrigger";
			case PadControl::RightTrigger: return "+RightTrigger";
			}
			return "Guide";
		}

		const char* hotkeyName(util::HotkeyAction action)
		{
			using util::HotkeyAction;
			switch (action)
			{
			case HotkeyAction::Exit: return "PowerOff";
			case HotkeyAction::Menu: return "OpenPauseMenu";
			case HotkeyAction::Reset: return "Reset";
			case HotkeyAction::LoadState: return "LoadSelectedSaveState";
			case HotkeyAction::SaveState: return "SaveSelectedSaveState";
			case HotkeyAction::NextSlot: return "SelectNextSaveStateSlot";
			case HotkeyAction::PreviousSlot: return "SelectPreviousSaveStateSlot";
			case HotkeyAction::Rewind: return "Rewind";
			case HotkeyAction::FastForward: return "FastForward";
			case HotkeyAction::Screenshot: return "Screenshot";
			}
			return "";
		}

		std::string lower(std::string s)
		{
			std::ranges::transform(s, s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return s;
		}

		std::string language()
		{
			const char* lang = std::getenv("LANG");
			std::string_view prefix = lang != nullptr ? std::string_view(lang).substr(0, 5) : std::string_view();
			for (const auto& [locale, code] : languages)
				if (prefix == locale)
					return code;
			return "en";
		}

		const char* boolString(bool value) { return value ? "true" : "false"; }

		// DuckStation resolves relative paths in an .m3u against its own folder: rewrite it with
		// absolute paths, next to its other runtime files
		fs::path absoluteM3u(const fs::path& m3u, const fs::path& dir)
		{
			std::ifstream in(m3u);
			std::vector<fs::path> discs;
			std::string line;
			while (std::getline(in, line))
			{
				while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
					line.pop_back();
				if (line.empty() || line.front() == '#')
					continue;

				fs::path disc(line);
				std::error_code ec;
				if (!(disc.is_absolute() && fs::exists(disc, ec)))
					disc = m3u.parent_path() / fs::path(line).relative_path();
				discs.push_back(disc);
			}

			if (discs.empty())
				return m3u;

			std::string content;
			for (const auto& disc : discs)
				content += disc.string() + "\n";

			fs::path rewritten = dir / m3u.filename();
			util::writeFileAtomically(rewritten, content);
			return rewritten;
		}
	}

	fs::path DuckStationGenerator::dataDirectory(const LaunchContext& ctx)
	{
		return ctx.paths.configFor("duckstation") / "duckstation";
	}

	std::map<std::string, std::string> DuckStationGenerator::findBios(const fs::path& dir)
	{
		std::map<std::string, std::string> lowerNames;   // lower case -> as on disk
		std::error_code ec;
		for (const auto& entry : fs::directory_iterator(dir, ec))
			if (entry.is_regular_file(ec))
				lowerNames.emplace(lower(entry.path().filename().string()), entry.path().filename().string());

		std::map<std::string, std::string> found;
		for (const auto& [region, files] : biosLists)
		{
			for (const char* file : files)
			{
				if (file == nullptr)
					break;
				auto it = lowerNames.find(file);
				if (it != lowerNames.end())
				{
					found.emplace(region, it->second);
					break;
				}
			}
		}
		return found;
	}

	void DuckStationGenerator::writeSettings(const LaunchContext& ctx, IniFile& ini)
	{
		const Settings& s = ctx.settings;

		// [Main]
		ini.set("Main", "SettingsVersion", "3");
		ini.set("Main", "InhibitScreensaver", "true");
		ini.set("Main", "StartPaused", "false");
		ini.set("Main", "StartFullscreen", "true");
		ini.set("Main", "PauseOnFocusLoss", "false");
		ini.set("Main", "PauseOnMenu", "true");
		ini.set("Main", "ConfirmPowerOff", "false");
		ini.set("Main", "ApplyGameSettings", "true");
		ini.set("Main", "SetupWizardIncomplete", "false");
		ini.set("Main", "EnableDiscordPresence", "false");
		ini.set("Main", "AutoEnableAnalog", "true");
		ini.set("Main", "EmulationSpeed", s.getString("duckstation_clocking", "1"));
		ini.set("Main", "SyncToHostRefreshRate", s.getString("duckstation_hrr", "false"));
		ini.set("Main", "Language", language());

		// rewind: the length the player chose, in seconds
		ini.set("Main", "RewindEnable", "true");
		ini.set("Main", "RewindFrequency", "1");
		std::string rewind = s.getString("duckstation_rewind");
		if (rewind == "120" || rewind == "90" || rewind == "60" || rewind == "30" || rewind == "15")
			ini.set("Main", "RewindSaveSlots", rewind);
		else if (rewind == "10")
		{
			ini.set("Main", "RewindSaveSlots", "100");
			ini.set("Main", "RewindFrequency", "0.100000");
		}
		else if (rewind == "5")
		{
			ini.set("Main", "RewindSaveSlots", "50");
			ini.set("Main", "RewindFrequency", "0.050000");
		}
		else
			ini.set("Main", "RewindEnable", "false");

		// [Console]
		ini.set("Console", "Region", s.getString("duckstation_region", "Auto"));
		ini.set("Console", "EnableCheats", s.getString("duckstation_cheats", "false"));

		// [BIOS]: at the root of the BIOS folder, as most BIOS packs and batocera do, else in psx/
		fs::path biosDir = ctx.paths.bios;
		auto bios = findBios(biosDir);
		if (bios.empty())
		{
			biosDir = ctx.paths.bios / "psx";
			bios = findBios(biosDir);
		}
		if (bios.empty())
			throw LaunchError(ErrorCode::Generic, "No PlayStation BIOS found in " + ctx.paths.bios.string() + " (scph1001.bin, scph5501.bin...).");

		ini.set("BIOS", "SearchDirectory", biosDir.string());
		ini.set("BIOS", "PatchFastBoot", s.getString("duckstation_PatchFastBoot", "false"));
		auto universal = bios.find("Uni");
		for (const auto& [region, key] : biosKeys)
		{
			if (universal != bios.end())
				ini.set("BIOS", key, universal->second);
			else if (auto it = bios.find(region); it != bios.end())
				ini.set("BIOS", key, it->second);
			else
				ini.remove("BIOS", key);
		}

		// [CPU]
		ini.set("CPU", "ExecutionMode", s.getString("duckstation_executionmode", "Recompiler"));

		// [GPU]
		ini.set("GPU", "Renderer", s.getString("duckstation_gfxbackend", "OpenGL"));
		ini.set("GPU", "ResolutionScale", s.getString("duckstation_resolution_scale", "1"));
		ini.set("GPU", "WidescreenHack", s.getString("duckstation_widescreen_hack", "false"));
		ini.set("GPU", "ForceNTSCTimings", s.getString("duckstation_60hz", "false"));
		ini.set("GPU", "TextureFilter", s.getString("duckstation_texture_filtering", "Nearest"));
		std::string pgxp = s.getString("duckstation_pgxp", "true");
		ini.set("GPU", "PGXPEnable", pgxp);
		ini.set("GPU", "PGXPCulling", pgxp);
		ini.set("GPU", "PGXPTextureCorrection", pgxp);
		ini.set("GPU", "PGXPPreserveProjFP", pgxp);
		ini.set("GPU", "PGXPDepthBuffer", s.getString("duckstation_pgxp_depth", "false"));
		ini.set("GPU", "TrueColor", s.getString("duckstation_truecolour", "false"));
		ini.set("GPU", "ScaledDithering", s.getString("duckstation_dithering", "true"));
		ini.set("GPU", "DisableInterlacing", s.getString("duckstation_interlacing", "false"));

		// "4" is 4x MSAA, "4-ssaa" 4x SSAA
		std::string antialiasing = s.getString("duckstation_antialiasing", "1");
		bool ssaa = antialiasing.find("ssaa") != std::string::npos;
		ini.set("GPU", "PerSampleShading", boolString(ssaa));
		ini.set("GPU", "Multisamples", antialiasing.substr(0, antialiasing.find('-')));

		// [Display]
		std::string ratio = s.getString("duckstation_ratio");
		bool stretch = s.getBool("duckstation_stretch", false);
		if (stretch || ratio == "Auto (Match Window)")
			ini.set("Display", "AspectRatio", "Stretch To Fill");
		else
			ini.set("Display", "AspectRatio", ratio.empty() ? "Auto (Game Native)" : ratio);

		ini.set("Display", "VSync", s.getString("duckstation_vsync", "false"));
		ini.set("Display", "CropMode", s.getString("duckstation_CropMode", "Overscan"));
		ini.set("Display", "ShowOSDMessages", s.getString("duckstation_osd", "false"));
		ini.set("Display", "OptimalFramePacing", s.getString("duckstation_ofp", "false"));

		auto integer = s.get("duckstation_integer");
		auto linear = s.get("duckstation_linear");
		if (!integer && !linear)
			ini.remove("Display", "Scaling");
		else if (integer == "true")
			ini.set("Display", "Scaling", linear == "false" ? "NearestInteger" : "BilinearInteger");
		else
			ini.set("Display", "Scaling", linear == "false" ? "Nearest" : "BilinearSmooth");

		// [Audio]
		ini.set("Audio", "StretchMode", s.getString("duckstation_audio_mode", "TimeStretch"));

		// [GameList]
		ini.set("GameList", "RecursivePaths", ctx.game.rom.parent_path().string());

		// [Cheevos]
		if (s.getBool("retroachievements", false))
		{
			ini.set("Cheevos", "Enabled", "true");
			ini.set("Cheevos", "Username", s.getString("retroachievements.username"));
			ini.set("Cheevos", "Token", s.getString("retroachievements.token"));
			ini.set("Cheevos", "ChallengeMode", boolString(s.getBool("retroachievements.hardcore", false)));
			ini.set("Cheevos", "Overlays", boolString(s.getBool("retroachievements.challenge_indicators", false)));
			ini.set("Cheevos", "LeaderboardNotifications", boolString(s.getBool("retroachievements.leaderboards", false)));
			ini.set("Cheevos", "EncoreMode", boolString(s.getBool("retroachievements.encore", false)));
			ini.set("Cheevos", "UnofficialTestMode", boolString(s.getBool("retroachievements.unofficial", false)));
		}
		else
			ini.set("Cheevos", "Enabled", "false");

		// [TextureReplacements]: in <data>/textures/<game id>
		std::string textures = s.getString("duckstation_custom_textures");
		ini.set("TextureReplacements", "EnableVRAMWriteReplacements", boolString(textures != "false" && textures != "0"));
		ini.set("TextureReplacements", "PreloadTextures", boolString(textures == "preload"));

		// [InputSources]
		ini.set("InputSources", "SDL", "true");
		ini.set("InputSources", "SDLControllerEnhancedMode", "false");
		ini.set("InputSources", "Evdev", "false");
		ini.set("InputSources", "XInput", "false");
		ini.set("InputSources", "RawInput", "false");

		// [MemoryCards] and [Folders]: absolute, so they do not depend on where the data folder is
		fs::path saves = ctx.paths.saves / "psx";
		ini.set("MemoryCards", "Directory", saves.string());
		ini.set("Folders", "SaveStates", (saves / "duckstation").string());
		ini.set("Folders", "Screenshots", ctx.paths.screenshots.string());
		ini.set("Folders", "Cache", (ctx.paths.cache / "duckstation").string());

		// [CDROM]
		ini.set("CDROM", "AllowBootingWithoutSBIFile", s.getString("duckstation_boot_without_sbi", "false"));

		// [UI]
		ini.set("UI", "UnofficialBuildWarningConfirmed", "true");

		writeControllers(ctx, ini);
	}

	void DuckStationGenerator::writeControllers(const LaunchContext& ctx, IniFile& ini)
	{
		const Settings& s = ctx.settings;

		for (int player = 1; player <= 8; ++player)
		{
			std::string section = "Pad" + std::to_string(player);
			ini.removeSection(section);
			ini.set(section, "Type", "None");
		}

		ini.set("ControllerPorts", "ControllerSettingsMigrated", "true");
		ini.set("ControllerPorts", "MultitapMode", "Disabled");
		ini.set("ControllerPorts", "PointerXScale", "8");
		ini.set("ControllerPorts", "PointerYScale", "8");
		ini.set("ControllerPorts", "PointerXInvert", "false");
		ini.set("ControllerPorts", "PointerYInvert", "false");

		int player = 0;
		for (const Controller& pad : ctx.controllers)
		{
			if (++player > 8)
				break;

			// a multitap for players 3 and more
			if (player > 2)
				ini.set("ControllerPorts", "MultitapMode", player > 4 ? "BothPorts" : "Port1Only");

			std::string section = "Pad" + std::to_string(player);
			std::string sdl = "SDL-" + std::to_string(pad.deviceIndex);
			std::string type = s.getString("duckstation_Controller" + std::to_string(player), "DigitalController");
			ini.set(section, "Type", type);

			for (const auto& [option, binding] : padBindings)
				ini.set(section, option, sdl + "/" + binding);
			ini.set(section, "VibrationBias", "8");

			std::string digitalMode = s.getString("duckstation_digitalmode", "false");
			ini.set(section, "AnalogDPadInDigitalMode", digitalMode);
			if (digitalMode != "false" && type == "AnalogController")
				ini.set(section, "Analog", sdl + "/Guide");

			if (type == "NeGcon" || type == "NeGconRumble")
				for (const auto& [option, binding] : negconBindings)
					ini.set(section, option, sdl + "/" + binding);

			if (type == "PlayStationMouse")
			{
				ini.set(section, "Right", sdl + "/B");
				ini.set(section, "Left", sdl + "/A");
			}
		}

		// [Hotkeys]: the hotkey of player 1 held with another control; the keyboard without a pad
		ini.removeSection("Hotkeys");
		if (const Controller* pad = util::firstPlayer(ctx))
		{
			std::string sdl = "SDL-" + std::to_string(pad->deviceIndex) + "/";
			std::string hotkey = sdl + controlName(util::hotkeyControl(*pad));
			for (const auto& [action, control] : util::hotkeys)
				ini.set("Hotkeys", hotkeyName(action), hotkey + " & " + sdl + controlName(control));
		}
		else
		{
			for (const auto& [name, key] : keyboardHotkeys)
				ini.set("Hotkeys", name, key);
		}
	}

	Command DuckStationGenerator::configure(const LaunchContext& ctx)
	{
		fs::path home = ctx.paths.configFor("duckstation");
		fs::path saves = ctx.paths.saves / "psx";

		auto program = util::emulatorCommand(ctx, "duckstation", { "duckstation-qt", "duckstation" }, {
			home, saves, ctx.paths.bios, ctx.paths.screenshots, ctx.paths.cache, ctx.game.rom.parent_path() });
		if (!program)
			throw LaunchError(ErrorCode::EmulatorMissing, "DuckStation is not installed: install it from the emulator catalog or with your package manager.");

		fs::path settingsPath = dataDirectory(ctx) / "settings.ini";
		IniFile settings = IniFile::load(settingsPath);
		writeSettings(ctx, settings);

		fs::path rom = ctx.game.rom;
		try
		{
			for (const fs::path& dir : { saves, saves / "duckstation", ctx.paths.screenshots, ctx.paths.cache / "duckstation" })
				fs::create_directories(dir);

			settings.save(settingsPath);

			if (rom.extension() == ".m3u")
				rom = absoluteM3u(rom, home / "m3u");
		}
		catch (const fs::filesystem_error& e)
		{
			throw LaunchError(ErrorCode::InvalidConfiguration, std::string("Cannot write the DuckStation configuration: ") + e.what());
		}

		Command cmd;
		cmd.argv = *program;
		cmd.argv.insert(cmd.argv.end(), { "-batch", "-nogui", "-fullscreen" });
		if (!ctx.saveState.file.empty())
			cmd.argv.insert(cmd.argv.end(), { "-statefile", ctx.saveState.file.string() });
		cmd.argv.insert(cmd.argv.end(), { "--", rom.string() });

		cmd.env["XDG_CONFIG_HOME"] = home.string();
		cmd.env["SDL_GAMECONTROLLERCONFIG"] = util::gameControllerConfig(ctx.controllers);
		util::forwardEnvironment(cmd);
		return cmd;
	}
}
