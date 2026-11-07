// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch-pcsx2, Copyright (c) 2026 The Batocera Team.

#include "pcsx2/Pcsx2Generator.h"

#include "es-launcher/LaunchError.h"
#include "util/EmulatorCommand.h"
#include "util/Gamepad.h"

#include <algorithm>
#include <vector>

namespace es::launcher::pcsx2
{
	namespace fs = std::filesystem;
	using util::IniFile;
	using util::PadControl;

	ES_LAUNCHER_REGISTER_GENERATOR("pcsx2", Pcsx2Generator)

	namespace
	{
		// DualShock 2 control -> SDL gamepad control, as PCSX2 names it
		constexpr std::pair<const char*, const char*> padBindings[] = {
			{ "Up", "DPadUp" }, { "Right", "DPadRight" }, { "Down", "DPadDown" }, { "Left", "DPadLeft" },
			{ "Triangle", "FaceNorth" }, { "Circle", "FaceEast" }, { "Cross", "FaceSouth" }, { "Square", "FaceWest" },
			{ "Select", "Back" }, { "Start", "Start" },
			{ "L1", "LeftShoulder" }, { "L2", "+LeftTrigger" }, { "R1", "RightShoulder" }, { "R2", "+RightTrigger" },
			{ "L3", "LeftStick" }, { "R3", "RightStick" },
			{ "LUp", "-LeftY" }, { "LRight", "+LeftX" }, { "LDown", "+LeftY" }, { "LLeft", "-LeftX" },
			{ "RUp", "-RightY" }, { "RRight", "+RightX" }, { "RDown", "+RightY" }, { "RLeft", "-RightX" },
			{ "Analog", "Guide" },
			{ "LargeMotor", "LargeMotor" }, { "SmallMotor", "SmallMotor" },
		};

		constexpr std::pair<const char*, const char*> padOptions[] = {
			{ "Type", "DualShock2" }, { "InvertL", "0" }, { "InvertR", "0" }, { "Deadzone", "0" },
			{ "AxisScale", "1.33" }, { "TriggerDeadzone", "0" }, { "TriggerScale", "1" },
			{ "LargeMotorScale", "1" }, { "SmallMotorScale", "1" }, { "ButtonDeadzone", "0" },
			{ "PressureModifier", "0.5" },
		};

		// Sony pads SDL drives better in its enhanced mode (batocera-launch's list)
		constexpr const char* sonyGuids[] = {
			"030000004c0500006802000011010000", "030000004c0500006802000011810000", "050000004c0500006802000000800000",
			"050000004c0500006802000000000000", "030000004c050000c405000011810000", "050000004c050000c405000000810000",
			"030000004c050000cc09000011010000", "050000004c050000cc09000000010000", "030000004c050000cc09000011810000",
			"050000004c050000cc09000000810000", "030000004c050000a00b000011010000", "030000004c050000a00b000011810000",
			"030000004c050000e60c000011810000", "050000004c050000e60c000000810000",
		};

		// Without a pad, the keyboard keys batocera-launch uses
		constexpr std::pair<const char*, const char*> keyboardHotkeys[] = {
			{ "ShutdownVM", "Keyboard/Escape" },
			{ "OpenPauseMenu", "Keyboard/F10" },
			{ "LoadStateFromSlot", "Keyboard/F3" },
			{ "SaveStateToSlot", "Keyboard/F1" },
			{ "NextSaveStateSlot", "Keyboard/F2" },
			{ "PreviousSaveStateSlot", "Keyboard/Shift & Keyboard/F2" },
			{ "ToggleTurbo", "Keyboard/Tab" },
			{ "Screenshot", "Keyboard/F8" },
		};

		const char* controlName(PadControl control)
		{
			switch (control)
			{
			case PadControl::South: return "FaceSouth";
			case PadControl::East: return "FaceEast";
			case PadControl::West: return "FaceWest";
			case PadControl::North: return "FaceNorth";
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

		// PCSX2 has no rewind
		const char* hotkeyName(util::HotkeyAction action)
		{
			using util::HotkeyAction;
			switch (action)
			{
			case HotkeyAction::Exit: return "ShutdownVM";
			case HotkeyAction::Menu: return "OpenPauseMenu";
			case HotkeyAction::Reset: return "ResetVM";
			case HotkeyAction::LoadState: return "LoadStateFromSlot";
			case HotkeyAction::SaveState: return "SaveStateToSlot";
			case HotkeyAction::NextSlot: return "NextSaveStateSlot";
			case HotkeyAction::PreviousSlot: return "PreviousSaveStateSlot";
			case HotkeyAction::FastForward: return "HoldTurbo";
			case HotkeyAction::Screenshot: return "Screenshot";
			case HotkeyAction::Rewind: return nullptr;
			}
			return nullptr;
		}

		const char* boolString(bool value) { return value ? "true" : "false"; }

		bool isBiosImage(const fs::path& path)
		{
			std::error_code ec;
			if (!fs::is_regular_file(path, ec))
				return false;
			auto size = fs::file_size(path, ec);
			return !ec && size >= 4 * 1024 * 1024 && size <= 8 * 1024 * 1024;
		}
	}

	fs::path Pcsx2Generator::dataDirectory(const LaunchContext& ctx)
	{
		return ctx.paths.configFor("pcsx2") / "PCSX2";
	}

	std::optional<fs::path> Pcsx2Generator::findBios(const fs::path& dir, const std::string& preferred)
	{
		if (!preferred.empty() && isBiosImage(dir / preferred))
			return dir / preferred;

		std::vector<fs::path> images;
		std::error_code ec;
		for (const auto& entry : fs::directory_iterator(dir, ec))
			if (isBiosImage(entry.path()))
				images.push_back(entry.path());

		if (images.empty())
			return std::nullopt;

		// the same one at every launch
		return *std::ranges::min_element(images);
	}

	void Pcsx2Generator::writeSettings(const LaunchContext& ctx, IniFile& ini)
	{
		const Settings& s = ctx.settings;

		// [UI]
		ini.set("UI", "SettingsVersion", "1");
		ini.set("UI", "SetupWizardIncomplete", "false");
		ini.set("UI", "InhibitScreensaver", "true");
		ini.set("UI", "ConfirmShutdown", "false");
		ini.set("UI", "StartPaused", "false");
		ini.set("UI", "PauseOnFocusLoss", "false");
		ini.set("UI", "StartFullscreen", "true");
		ini.set("UI", "HideMouseCursor", "true");
		ini.set("UI", "RenderToSeparateWindow", "false");
		ini.set("UI", "HideMainWindowWhenRunning", "true");
		ini.set("UI", "DoubleClickTogglesFullscreen", "false");
		for (const char* geometry : { "MainWindowGeometry", "MainWindowState", "DisplayWindowGeometry" })
			ini.remove("UI", geometry);

		// [Folders] and [Filenames]: the BIOS in <bios>/ps2 like batocera, else at the root
		fs::path biosDir = ctx.paths.bios / "ps2";
		std::string preferred = ini.get("Filenames", "BIOS").value_or("");
		auto bios = findBios(biosDir, preferred);
		if (!bios)
		{
			biosDir = ctx.paths.bios;
			bios = findBios(biosDir, preferred);
		}
		if (!bios)
			throw LaunchError(ErrorCode::Generic, "No PlayStation 2 BIOS found in " + (ctx.paths.bios / "ps2").string() + ".");

		fs::path saves = ctx.paths.saves / "ps2";
		ini.remove("Folders", "SaveStates");   // an old spelling
		ini.set("Folders", "Bios", biosDir.string());
		ini.set("Filenames", "BIOS", bios->filename().string());
		ini.set("Folders", "Snapshots", ctx.paths.screenshots.string());
		ini.set("Folders", "Savestates", (saves / "pcsx2" / "sstates").string());
		ini.set("Folders", "MemoryCards", (saves / "pcsx2").string());
		ini.set("Folders", "Cache", (ctx.paths.cache / "pcsx2").string());
		ini.set("Folders", "Videos", (saves / "pcsx2" / "videos").string());

		// [EmuCore]
		ini.set("EmuCore", "EnableDiscordPresence", "false");
		ini.set("EmuCore", "EnableFastBoot", boolString(s.getBool("pcsx2_fastboot", true)));
		ini.set("EmuCore", "EnableCheats", s.getString("pcsx2_cheats", "false"));
		ini.set("EmuCore", "EnableWideScreenPatches", s.getString("pcsx2_EnableWideScreenPatches", "false"));
		ini.set("EmuCore", "EnableNoInterlacingPatches", s.getString("pcsx2_interlacing_patches", "false"));
		ini.set("EmuCore", "AutoIncrementSlot", boolString(s.getBool("incrementalsavestates", true)));
		ini.set("EmuCore", "SaveStateOnShutdown", boolString(s.getBool("autosave", false)));

		// [EmuCore/Speedhacks]
		ini.set("EmuCore/Speedhacks", "vuThread", boolString(s.getBool("pcsx2_vuthread", false)));
		ini.set("EmuCore/Speedhacks", "EECycleRate", s.getString("pcsx2_eecyclerate", "0"));

		// [Achievements]
		if (s.getBool("retroachievements", false))
		{
			bool leaderboards = s.getBool("retroachievements.leaderboards", false);
			ini.set("Achievements", "Enabled", "true");
			ini.set("Achievements", "Username", s.getString("retroachievements.username"));
			ini.set("Achievements", "Token", s.getString("retroachievements.token"));
			ini.set("Achievements", "ChallengeMode", boolString(s.getBool("retroachievements.hardcore", false)));
			ini.set("Achievements", "Overlays", boolString(s.getBool("retroachievements.challenge_indicators", false)));
			ini.set("Achievements", "LeaderboardNotifications", boolString(leaderboards));
			ini.set("Achievements", "LBOverlays", boolString(leaderboards));
			ini.set("Achievements", "EncoreMode", boolString(s.getBool("retroachievements.encore", false)));
			ini.set("Achievements", "UnofficialTestMode", boolString(s.getBool("retroachievements.unofficial", false)));
		}
		else
			ini.set("Achievements", "Enabled", "false");
		ini.set("Achievements", "Notifications", "true");
		ini.set("Achievements", "SoundEffects", "true");

		// [EmuCore/GS]: -1 automatic, 12 OpenGL, 13 software, 14 Vulkan
		ini.set("EmuCore/GS", "Renderer", s.getString("pcsx2_gfxbackend", "-1"));
		ini.set("EmuCore/GS", "AspectRatio", s.getString("pcsx2_ratio", "Auto 4:3/3:2"));
		ini.set("EmuCore/GS", "VsyncEnable", s.getString("pcsx2_vsync", "0"));
		ini.set("EmuCore/GS", "upscale_multiplier", s.getString("pcsx2_resolution", "1"));
		ini.set("EmuCore/GS", "fxaa", s.getString("pcsx2_fxaa", "false"));
		ini.set("EmuCore/GS", "FMVAspectRatioSwitch", s.getString("pcsx2_fmv_ratio", "Off"));
		std::string mipmap = boolString(s.getString("pcsx2_mipmapping") != "0");
		ini.set("EmuCore/GS", "mipmap", mipmap);
		ini.set("EmuCore/GS", "hw_mipmap", mipmap);
		ini.set("EmuCore/GS", "TriFilter", s.getString("pcsx2_trilinear_filtering", "-1"));
		ini.set("EmuCore/GS", "MaxAnisotropy", s.getString("pcsx2_anisotropic_filtering", "0"));
		ini.set("EmuCore/GS", "dithering_ps2", s.getString("pcsx2_dithering", "2"));
		ini.set("EmuCore/GS", "texture_preloading", s.getString("pcsx2_texture_loading", "2"));
		ini.set("EmuCore/GS", "deinterlace_mode", s.getString("pcsx2_deinterlacing", "0"));
		ini.set("EmuCore/GS", "pcrtc_antiblur", s.getString("pcsx2_blur", "true"));
		ini.set("EmuCore/GS", "IntegerScaling", s.getString("pcsx2_scaling", "false"));
		ini.set("EmuCore/GS", "accurate_blending_unit", s.getString("pcsx2_blending", "1"));
		ini.set("EmuCore/GS", "filter", s.getString("pcsx2_texture_filtering", "2"));
		ini.set("EmuCore/GS", "linear_present_mode", s.getString("pcsx2_bilinear_filtering", "1"));
		ini.set("EmuCore/GS", "LoadTextureReplacements", s.getString("pcsx2_texture_replacements", "false"));
		ini.set("EmuCore/GS", "OsdMessagesPos",
			s.getString("pcsx2_osd_messages") == "false" ? "0" : s.getString("pcsx2_osd_messages_position", "2"));
		ini.set("EmuCore/GS", "OsdPerformancePos", s.getString("pcsx2_osd_performance_position", "0"));
		std::string crop = s.getBool("pcsx2_crop_overscan", false) ? "3" : "0";
		for (const char* side : { "CropLeft", "CropTop", "CropRight", "CropBottom" })
			ini.set("EmuCore/GS", side, crop);
		ini.set("EmuCore/GS", "TVShader", s.getString("pcsx2_shaderset", "0"));

		// [GameList]
		ini.set("GameList", "RecursivePaths", ctx.game.rom.parent_path().string());

		writeControllers(ctx, ini);
	}

	void Pcsx2Generator::writeControllers(const LaunchContext& ctx, IniFile& ini)
	{
		ini.set("InputSources", "Keyboard", "true");
		ini.set("InputSources", "Mouse", "true");
		ini.set("InputSources", "SDL", "true");

		// a multitap on port 1 for 4 players, on both ports for 8, when the option asks for it
		int pads = static_cast<int>(ctx.controllers.size());
		std::string multitap = ctx.settings.getString("pcsx2_multitap");
		int slots = 2;
		if (multitap == "4" && pads > 2)
			slots = 4;
		else if (multitap == "8" && pads > 2)
			slots = pads > 4 ? 8 : 4;
		ini.set("Pad", "MultitapPort1", boolString(slots >= 4));
		ini.set("Pad", "MultitapPort2", boolString(slots == 8));

		for (int i = 1; i <= 8; ++i)
			ini.removeSection("Pad" + std::to_string(i));

		bool sony = false;
		int player = 0;
		for (const Controller& pad : ctx.controllers)
		{
			if (++player > slots)
				break;

			// with a multitap on port 1 only, its slots are pads 1, 3, 4 and 5: pad 2 is port 2
			int index = (slots == 4 && player > 1) ? player + 1 : player;
			std::string section = "Pad" + std::to_string(index);
			std::string sdl = "SDL-" + std::to_string(pad.deviceIndex) + "/";

			for (const auto& [option, value] : padOptions)
				ini.set(section, option, value);
			for (const auto& [option, binding] : padBindings)
				ini.set(section, option, sdl + binding);

			sony = sony || std::ranges::find(sonyGuids, pad.guid) != std::end(sonyGuids);
		}
		ini.set("InputSources", "SDLControllerEnhancedMode", boolString(sony));

		// [Hotkeys]: the hotkey of player 1 held with another control; the keyboard without a pad
		ini.removeSection("Hotkeys");
		if (const Controller* pad = util::firstPlayer(ctx))
		{
			std::string sdl = "SDL-" + std::to_string(pad->deviceIndex) + "/";
			std::string hotkey = sdl + controlName(util::hotkeyControl(*pad));
			for (const auto& [action, control] : util::hotkeys)
				if (const char* name = hotkeyName(action))
					ini.set("Hotkeys", name, hotkey + " & " + sdl + controlName(control));
		}
		else
		{
			for (const auto& [name, key] : keyboardHotkeys)
				ini.set("Hotkeys", name, key);
		}
	}

	Command Pcsx2Generator::configure(const LaunchContext& ctx)
	{
		if (ctx.architecture != Architecture::X86_64)
			throw LaunchError(ErrorCode::Generic, "PCSX2 only runs on x86_64 computers.");

		fs::path home = ctx.paths.configFor("pcsx2");
		fs::path saves = ctx.paths.saves / "ps2";

		auto program = util::emulatorCommand(ctx, "pcsx2", { "pcsx2-qt", "pcsx2" }, {
			home, saves, ctx.paths.bios, ctx.paths.screenshots, ctx.paths.cache, ctx.game.rom.parent_path() });
		if (!program)
			throw LaunchError(ErrorCode::EmulatorMissing, "PCSX2 is not installed: install it from the emulator catalog or with your package manager.");

		fs::path iniPath = dataDirectory(ctx) / "inis" / "PCSX2.ini";
		IniFile ini = IniFile::load(iniPath);
		writeSettings(ctx, ini);

		try
		{
			for (const fs::path& dir : { saves / "pcsx2" / "sstates", ctx.paths.screenshots, ctx.paths.cache / "pcsx2" })
				fs::create_directories(dir);
			ini.save(iniPath);
		}
		catch (const fs::filesystem_error& e)
		{
			throw LaunchError(ErrorCode::InvalidConfiguration, std::string("Cannot write the PCSX2 configuration: ") + e.what());
		}

		Command cmd;
		cmd.argv = *program;
		cmd.argv.insert(cmd.argv.end(), { "-nogui", "-fullscreen" });
		if (!ctx.saveState.file.empty())
			cmd.argv.insert(cmd.argv.end(), { "-statefile", ctx.saveState.file.string() });
		cmd.argv.insert(cmd.argv.end(), { "--", ctx.game.rom.string() });

		cmd.env["XDG_CONFIG_HOME"] = home.string();
		cmd.env["SDL_GAMECONTROLLERCONFIG"] = util::gameControllerConfig(ctx.controllers);
		util::forwardEnvironment(cmd);
		return cmd;
	}
}
