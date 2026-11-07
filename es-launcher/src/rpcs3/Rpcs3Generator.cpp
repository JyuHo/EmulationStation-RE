// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch-rpcs3, Copyright (c) 2026 The Batocera Team.

#include "rpcs3/Rpcs3Generator.h"

#include "es-launcher/LaunchError.h"
#include "util/EmulatorCommand.h"
#include "util/Files.h"
#include "util/Gamepad.h"
#include "util/IniFile.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <map>

namespace es::launcher::rpcs3
{
	namespace fs = std::filesystem;
	using util::YamlFile;

	ES_LAUNCHER_REGISTER_GENERATOR("rpcs3", Rpcs3Generator)

	namespace
	{
		// Shader Mode values saved before RPCS3 renamed them
		constexpr std::pair<std::string_view, std::string_view> legacyShaderModes[] = {
			{ "Async with Shader Interpreter", "Async Recompiler with Shader Interpreter" },
			{ "Async Shader Recompiler", "Async Recompiler (multi-threaded)" },
			{ "Shader Recompiler", "Legacy Recompiler (single-threaded)" },
		};

		constexpr std::string_view frameLimits[] = { "Off", "30", "50", "60", "120", "Display", "Auto", "PS3 Native", "Infinite" };

		// Sony pads RPCS3 drives through its own handlers (rpcs3_controllerN = Sony): GUID -> handler, device
		struct SonyPad
		{
			std::string_view guid;
			std::string_view handler;
			std::string_view device;
		};
		constexpr SonyPad sonyPads[] = {
			{ "030000004c0500006802000011010000", "DualShock 3", "DS3 Pad" },
			{ "030000004c0500006802000011810000", "DualShock 3", "DS3 Pad" },
			{ "050000004c0500006802000000800000", "DualShock 3", "DS3 Pad" },
			{ "050000004c0500006802000000000000", "DualShock 3", "DS3 Pad" },
			{ "030000004c050000c405000011810000", "DualShock 4", "DS4 Pad" },
			{ "050000004c050000c405000000810000", "DualShock 4", "DS4 Pad" },
			{ "030000004c050000cc09000011010000", "DualShock 4", "DS4 Pad" },
			{ "050000004c050000cc09000000010000", "DualShock 4", "DS4 Pad" },
			{ "030000004c050000cc09000011810000", "DualShock 4", "DS4 Pad" },
			{ "050000004c050000cc09000000810000", "DualShock 4", "DS4 Pad" },
			{ "030000004c050000a00b000011010000", "DualShock 4", "DS4 Pad" },
			{ "030000004c050000a00b000011810000", "DualShock 4", "DS4 Pad" },
			{ "030000004c050000e60c000011810000", "DualSense", "DualSense Pad" },
			{ "050000004c050000e60c000000810000", "DualSense", "DualSense Pad" },
		};

		// The PS3 buttons on an SDL gamepad: SDL_GAMECONTROLLERCONFIG gives ES's mapping, so
		// these names are the same for every pad
		constexpr std::pair<const char*, const char*> sdlButtons[] = {
			{ "Left Stick Left", "LS X-" }, { "Left Stick Down", "LS Y-" }, { "Left Stick Right", "LS X+" }, { "Left Stick Up", "LS Y+" },
			{ "Right Stick Left", "RS X-" }, { "Right Stick Down", "RS Y-" }, { "Right Stick Right", "RS X+" }, { "Right Stick Up", "RS Y+" },
			{ "Start", "Start" }, { "Select", "Back" }, { "PS Button", "Guide" },
			{ "Square", "West" }, { "Cross", "South" }, { "Circle", "East" }, { "Triangle", "North" },
			{ "Left", "Left" }, { "Down", "Down" }, { "Right", "Right" }, { "Up", "Up" },
			{ "R1", "RB" }, { "R2", "RT" }, { "R3", "RS" }, { "L1", "LB" }, { "L2", "LT" }, { "L3", "LS" },
		};

		constexpr std::pair<const char*, const char*> sonyButtons[] = {
			{ "Left Stick Left", "LS X-" }, { "Left Stick Down", "LS Y-" }, { "Left Stick Right", "LS X+" }, { "Left Stick Up", "LS Y+" },
			{ "Right Stick Left", "RS X-" }, { "Right Stick Down", "RS Y-" }, { "Right Stick Right", "RS X+" }, { "Right Stick Up", "RS Y+" },
			{ "Start", "Options" }, { "Select", "Share" }, { "PS Button", "PS Button" },
			{ "Square", "Square" }, { "Cross", "Cross" }, { "Circle", "Circle" }, { "Triangle", "Triangle" },
			{ "Left", "Left" }, { "Down", "Down" }, { "Right", "Right" }, { "Up", "Up" },
			{ "R1", "R1" }, { "R2", "R2" }, { "R3", "R3" }, { "L1", "L1" }, { "L2", "L2" }, { "L3", "L3" },
		};

		const SonyPad* sonyPad(std::string_view guid)
		{
			for (const SonyPad& pad : sonyPads)
				if (pad.guid == guid)
					return &pad;
			return nullptr;
		}

		bool isNumber(std::string_view value)
		{
			double number = 0;
			auto [end, ec] = std::from_chars(value.data(), value.data() + value.size(), number);
			return ec == std::errc() && end == value.data() + value.size();
		}

		// A number option, kept as the player wrote it ("16", "0.5") when it is one
		void setNumber(YamlFile& yaml, YamlFile::Path path, const Settings& s, std::string_view key, std::string_view fallback)
		{
			std::string value = s.getString(key, fallback);
			yaml.setRaw(path, isNumber(value) ? value : std::string(fallback));
		}

		void setMotionSensors(YamlFile& yaml, std::string_view player, bool sdl)
		{
			constexpr std::pair<const char*, const char*> sensors[] = {
				{ "Motion Sensor X", "X" }, { "Motion Sensor Y", "Y" }, { "Motion Sensor Z", "Z" }, { "Motion Sensor G", "RY" } };
			for (const auto& [sensor, axis] : sensors)
			{
				yaml.set({ player, "Config", sensor, "Axis" }, sdl ? axis : "");
				yaml.set({ player, "Config", sensor, "Mirrored" }, false);
				yaml.set({ player, "Config", sensor, "Shift" }, 0);
			}
		}

		// The values RPCS3 writes for a pad, after the buttons
		void setPadTuning(YamlFile& yaml, std::string_view player, bool sdl, bool rumble)
		{
			auto set = [&](const char* key, auto value) { yaml.set({ player, "Config", key }, value); };

			set("Pressure Intensity Button", "");
			set("Pressure Intensity Percent", 50);
			set("Pressure Intensity Toggle Mode", false);
			if (sdl)
			{
				set("Pressure Intensity Deadzone", 0);
				set("Analog Limiter Button", "");
				set("Analog Limiter Toggle Mode", false);
			}
			set("Left Stick Multiplier", 100);
			set("Right Stick Multiplier", 100);
			set("Left Stick Deadzone", sdl ? 8000 : 40);
			set("Right Stick Deadzone", sdl ? 8000 : 40);
			if (sdl)
			{
				set("Left Stick Anti-Deadzone", 4259);
				set("Right Stick Anti-Deadzone", 4259);
			}
			set("Left Trigger Threshold", 0);
			set("Right Trigger Threshold", 0);
			set("Left Pad Squircling Factor", 8000);
			set("Right Pad Squircling Factor", 8000);
			set("Color Value R", 0);
			set("Color Value G", 0);
			set("Color Value B", 20);
			set("Blink LED when battery is below 20%", true);
			set("Use LED as a battery indicator", false);
			set("LED battery indicator brightness", 10);
			set("Player LED enabled", true);
			set("Large Vibration Motor Multiplier", rumble ? 100 : 0);
			set("Small Vibration Motor Multiplier", rumble ? 100 : 0);
			set("Switch Vibration Motors", false);
			set("Mouse Movement Mode", "Relative");
			set("Mouse Deadzone X Axis", 60);
			set("Mouse Deadzone Y Axis", 60);
			set("Mouse Acceleration X Axis", 200);
			set("Mouse Acceleration Y Axis", 250);
			set("Left Stick Lerp Factor", 100);
			set("Right Stick Lerp Factor", 100);
			set("Analog Button Lerp Factor", 100);
			set("Trigger Lerp Factor", 100);
			set("Device Class Type", 0);
			set("Vendor ID", 1356);
			set("Product ID", 616);
		}

		fs::path hdd0(const LaunchContext& ctx)
		{
			return ctx.paths.saves / "ps3" / "dev_hdd0";
		}

		std::optional<fs::path> firmwareUpdate(const LaunchContext& ctx)
		{
			return util::findFile("PS3UPDAT.PUP", { ctx.paths.bios, ctx.paths.bios / "ps3" });
		}

		// The file RPCS3 boots: EBOOT.BIN of a disc folder or of an installed PSN game, or an .iso
		fs::path bootFile(const LaunchContext& ctx)
		{
			const fs::path& rom = ctx.game.rom;
			std::error_code ec;

			std::string extension = rom.extension().string();
			std::ranges::transform(extension, extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

			if (extension == ".psn")
			{
				// the game id (NPUB12345), on a line of its own
				std::ifstream in(rom);
				std::string line, id;
				while (std::getline(in, line))
				{
					std::erase_if(line, [](unsigned char c) { return std::isspace(c); });
					if (line.size() >= 9)
						id = line;
				}
				if (id.empty())
					throw LaunchError(ErrorCode::BadArguments, "No PS3 game id found in " + rom.filename().string() + ".");
				std::ranges::transform(id, id.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
				return hdd0(ctx) / "game" / id / "USRDIR" / "EBOOT.BIN";
			}

			if (extension == ".iso" || !fs::is_directory(rom, ec))
				return rom;

			return rom / "PS3_GAME" / "USRDIR" / "EBOOT.BIN";
		}
	}

	fs::path Rpcs3Generator::configDirectory(const LaunchContext& ctx)
	{
		return ctx.paths.configFor("rpcs3") / "rpcs3";
	}

	void Rpcs3Generator::writeSettings(const LaunchContext& ctx, YamlFile& yaml)
	{
		const Settings& s = ctx.settings;

		// VFS mount points moved to vfs.yml: the stale ones go, the VFS settings RPCS3 still reads stay
		for (const std::string& key : yaml.keys({ "VFS" }))
			if (key.starts_with("/") || key.starts_with("$("))
				yaml.remove({ "VFS", key });

		for (const char* section : { "Core", "Video", "Audio", "Input/Output", "System", "Net", "Savestate", "Miscellaneous", "Log" })
			yaml.addMap({ section });

		// [Core]
		yaml.set({ "Core", "PPU Decoder" }, s.getString("rpcs3_ppudecoder", "Recompiler (LLVM)"));
		yaml.set({ "Core", "SPU Decoder" }, s.getString("rpcs3_spudecoder", "Recompiler (LLVM)"));
		yaml.set({ "Core", "SPU XFloat Accuracy" }, s.getString("rpcs3_spuxfloataccuracy", "Approximate"));
		yaml.set({ "Core", "SPU Cache" }, true);   // less stutter once an SPU module is cached
		yaml.set({ "Core", "Preferred SPU Threads" }, s.getInt("rpcs3_sputhreads", 0));
		yaml.set({ "Core", "SPU loop detection" }, s.getBool("rpcs3_spuloopdetection", false));
		yaml.set({ "Core", "SPU Block Size" }, s.getString("rpcs3_spublocksize", "Safe"));
		yaml.set({ "Core", "Thread Scheduler Mode" }, s.getString("rpcs3_thread_scheduler", "Operating System"));
		yaml.set({ "Core", "Clocks scale" }, s.getInt("rpcs3_clocks_scale", 100));
		yaml.set({ "Core", "Max CPU Preempt Count" }, s.getInt("rpcs3_maxcpu_preemptcount", 0));
		yaml.set({ "Core", "Sleep Timers Accuracy" }, s.getString("rpcs3_sleep_timers_accuracy", "As Host"));
		yaml.set({ "Core", "RSX FIFO Fetch Accuracy" }, s.getString("rpcs3_rsx_fifo", "Atomic"));

		// [Video]
		std::string backend = s.getString("rpcs3_gfxbackend", "Vulkan");
		yaml.set({ "Video", "Renderer" }, backend);
		// the TV mode of the PS3 itself, not the displayed ratio
		yaml.set({ "Video", "Aspect ratio" }, s.getString("rpcs3_ratio", "16:9"));

		std::string shaderMode = s.getString("rpcs3_shadermode", "Async Recompiler with Shader Interpreter");
		for (const auto& [legacy, current] : legacyShaderModes)
			if (shaderMode == legacy)
				shaderMode = current;
		yaml.set({ "Video", "Shader Mode" }, shaderMode);
		yaml.set({ "Video", "VSync Mode" }, s.getString("rpcs3_vsync", "Disabled"));
		yaml.set({ "Video", "Stretch To Display Area" }, s.getBool("rpcs3_stretchdisplay", false));

		// a frame limit RPCS3 names, else any number as the second frame limit
		std::string frameLimit = s.getString("rpcs3_framelimit", "Auto");
		if (std::ranges::find(frameLimits, frameLimit) != std::end(frameLimits))
		{
			yaml.set({ "Video", "Frame limit" }, frameLimit);
			yaml.setRaw({ "Video", "Second Frame Limit" }, "0");
		}
		else if (isNumber(frameLimit))
		{
			yaml.set({ "Video", "Frame limit" }, "Off");
			yaml.setRaw({ "Video", "Second Frame Limit" }, frameLimit);
		}
		else
		{
			yaml.set({ "Video", "Frame limit" }, "Auto");
			yaml.setRaw({ "Video", "Second Frame Limit" }, "0");
		}

		yaml.set({ "Video", "Write Color Buffers" }, s.getBool("rpcs3_colorbuffers", false));
		yaml.set({ "Video", "Read Color Buffers" }, s.getBool("rpcs3_read_colorbuffers", false));
		yaml.set({ "Video", "Disable Vertex Cache" }, s.getBool("rpcs3_vertexcache", false));
		yaml.set({ "Video", "Anisotropic Filter Override" }, s.getInt("rpcs3_anisotropic", 0));
		yaml.set({ "Video", "MSAA" }, s.getString("rpcs3_aa", "Auto"));

		std::string zcull = s.getString("rpcs3_zcull");
		yaml.set({ "Video", "Accurate ZCULL stats" }, zcull != "Approximate" && zcull != "Relaxed" && zcull != "Disable");
		yaml.set({ "Video", "Relaxed ZCULL Sync" }, zcull == "Relaxed");
		yaml.set({ "Video", "Disable ZCull Occlusion Queries" }, zcull == "Disable");

		yaml.set({ "Video", "Shader Precision" }, s.getString("rpcs3_shader", "Auto"));
		yaml.set({ "Video", "Resolution" }, "1280x720");   // the PS3's own; scaled below
		yaml.set({ "Video", "Resolution Scale" }, s.getInt("rpcs3_resolution_scale", 100));
		setNumber(yaml, { "Video", "Minimum Scalable Dimension" }, s, "rpcs3_resolution_scale_threshold", "16");
		yaml.set({ "Video", "Output Scaling Mode" }, s.getString("rpcs3_scaling", "Bilinear"));
		setNumber(yaml, { "Video", "FidelityFX CAS Sharpening Intensity" }, s, "rpcs3_fsr_sharpening", "50");
		yaml.set({ "Video", "Shader Compiler Threads" }, s.getInt("rpcs3_num_compilers", 0));
		yaml.set({ "Video", "Multithreaded RSX" }, s.getBool("rpcs3_rsx", false));
		yaml.set({ "Video", "Write Depth Buffer" }, s.getBool("rpcs3_write_depth_buffers", false));
		yaml.set({ "Video", "Read Depth Buffer" }, s.getBool("rpcs3_read_depth_buffers", false));
		yaml.set({ "Video", "Strict Rendering Mode" }, s.getBool("rpcs3_strict", false));
		yaml.set({ "Video", "Force CPU Blit" }, s.getBool("rpcs3_force_cpu_blit_emulation", false));
		yaml.remove({ "Video", "VSync" });   // replaced by VSync Mode

		if (backend == "Vulkan")
		{
			yaml.set({ "Video", "Vulkan", "Asynchronous Texture Streaming" }, s.getBool("rpcs3_async_texture", false));
			yaml.set({ "Video", "Vulkan", "Asynchronous Queue Scheduler" }, s.getString("rpcs3_vk_async_scheduler", "Safe"));
			yaml.remove({ "Video", "Vulkan", "Asynchronous Texture Streaming 2" });
		}

		// [Audio]
		bool timeStretch = s.getBool("rpcs3_timestretch", false);
		yaml.set({ "Audio", "Renderer" }, "Cubeb");
		yaml.set({ "Audio", "Master Volume" }, 100);
		yaml.set({ "Audio", "Audio Format" }, s.getString("rpcs3_audio_format", "Automatic"));
		yaml.set({ "Audio", "Convert to 16 bit" }, s.getBool("rpcs3_audio_16bit", false));
		yaml.set({ "Audio", "Enable Buffering" }, timeStretch || s.getBool("rpcs3_audiobuffer", true));
		yaml.set({ "Audio", "Desired Audio Buffer Duration" }, s.getInt("rpcs3_audiobuffer_duration", 34));
		yaml.set({ "Audio", "Enable Time Stretching" }, timeStretch);
		yaml.set({ "Audio", "Time Stretching Threshold" }, s.getInt("rpcs3_timestretch_threshold", 75));

		// [Input/Output]
		yaml.set({ "Input/Output", "Pad handler mode" }, s.getString("rpcs3_pad_mode", "Single-threaded"));
		yaml.set({ "Input/Output", "Show move cursor" }, s.getBool("rpcs3_crosshairs", false));
		if (yaml.get({ "Input/Output", "Move" }) == "Gun")
		{
			// left by batocera-launch's light gun setup, which ES does not do
			yaml.set({ "Input/Output", "Move" }, "Null");
			yaml.set({ "Input/Output", "Camera" }, "Null");
			yaml.set({ "Input/Output", "Camera type" }, "Unknown");
		}

		yaml.remove({ "System", "Console PSID" });   // set for the arcade titles only

		// [Miscellaneous]
		yaml.set({ "Miscellaneous", "Exit RPCS3 when process finishes" }, true);
		yaml.set({ "Miscellaneous", "Start games in fullscreen mode" }, true);
		yaml.set({ "Miscellaneous", "Show shader compilation hint" }, false);
		yaml.set({ "Miscellaneous", "Prevent display sleep while running games" }, true);
		yaml.set({ "Miscellaneous", "Show trophy popups" }, false);
	}

	YamlFile Rpcs3Generator::inputConfig(const LaunchContext& ctx)
	{
		const Settings& s = ctx.settings;
		YamlFile yaml;

		// RPCS3 numbers the devices of the same name: "Xbox Controller 1", "Xbox Controller 2"
		std::map<std::string, int> counts;

		for (size_t i = 0; i < ctx.controllers.size() && i < 7; ++i)
		{
			const Controller& pad = ctx.controllers[i];
			std::string n = std::to_string(i + 1);
			std::string player = "Player " + n + " Input";
			bool rumble = s.getBool("rpcs3_rumble" + n, true);

			const SonyPad* sony = sonyPad(pad.guid);
			if (sony != nullptr && s.getString("rpcs3_controller" + n) == "Sony")
			{
				int count = ++counts[std::string(sony->device)];
				yaml.set({ player, "Handler" }, sony->handler);
				yaml.set({ player, "Device" }, std::string(sony->device) + " #" + std::to_string(count));
				for (const auto& [button, control] : sonyButtons)
					yaml.set({ player, "Config", button }, control);
				setMotionSensors(yaml, player, false);
				setPadTuning(yaml, player, false, rumble);
			}
			else
			{
				// SDL names the pad as ES's mapping does, commas included: RPCS3 cannot have them
				std::string name = pad.name;
				std::ranges::replace(name, ',', '.');
				int count = ++counts[name];
				yaml.set({ player, "Handler" }, "SDL");
				yaml.set({ player, "Device" }, name + " " + std::to_string(count));
				for (const auto& [button, control] : sdlButtons)
					yaml.set({ player, "Config", button }, control);
				for (const char* unused : { "IR Nose", "IR Tail", "IR Left", "IR Right", "Tilt Left", "Tilt Right" })
					yaml.set({ player, "Config", unused }, "");
				setMotionSensors(yaml, player, true);
				yaml.set({ player, "Config", "Orientation Reset Button" }, "");
				yaml.set({ player, "Config", "Orientation Enabled" }, false);
				setPadTuning(yaml, player, true, rumble);
			}
			yaml.set({ player, "Buddy Device" }, "");
		}

		return yaml;
	}

	YamlFile Rpcs3Generator::vfsConfig(const LaunchContext& ctx)
	{
		YamlFile yaml;
		yaml.set({ "$(EmulatorDir)" }, "");
		yaml.set({ "/dev_hdd0/" }, hdd0(ctx).string() + "/");
		yaml.set({ "/dev_hdd1/" }, "$(EmulatorDir)dev_hdd1/");
		yaml.set({ "/dev_flash/" }, "$(EmulatorDir)dev_flash/");
		yaml.set({ "/dev_flash2/" }, "$(EmulatorDir)dev_flash2/");
		yaml.set({ "/dev_flash3/" }, "$(EmulatorDir)dev_flash3/");
		yaml.set({ "/dev_bdvd/" }, "$(EmulatorDir)dev_bdvd/");
		yaml.set({ "/games/" }, "$(EmulatorDir)games/");
		yaml.set({ "/app_home/" }, "");
		for (int i = 0; i < 8; ++i)
		{
			std::string slot = "/dev_usb00" + std::to_string(i);
			yaml.set({ "/dev_usb***/", slot, "Path" }, i == 0 ? "$(EmulatorDir)dev_usb000/" : "");
			for (const char* key : { "Serial", "VID", "PID" })
				yaml.set({ "/dev_usb***/", slot, key }, "");
		}
		return yaml;
	}

	Command Rpcs3Generator::configure(const LaunchContext& ctx)
	{
		if (ctx.architecture != Architecture::X86_64)
			throw LaunchError(ErrorCode::Generic, "RPCS3 only runs on x86_64 computers.");

		fs::path home = ctx.paths.configFor("rpcs3");
		fs::path config = configDirectory(ctx);
		fs::path boot = bootFile(ctx);

		std::error_code ec;
		if (!fs::exists(boot, ec))
			throw LaunchError(ErrorCode::BadArguments, "The PS3 game cannot be found: " + boot.string() + " is missing.");

		// the firmware is installed once, from the update file Sony publishes
		bool installed = fs::is_regular_file(config / "dev_flash" / "vsh" / "etc" / "version.txt", ec);
		std::optional<fs::path> firmware = firmwareUpdate(ctx);
		if (!installed && !firmware)
			throw LaunchError(ErrorCode::Generic, "PS3 firmware missing: copy PS3UPDAT.PUP into the BIOS folder.");

		auto program = util::emulatorCommand(ctx, "rpcs3", "rpcs3", {
			home, ctx.paths.saves / "ps3", ctx.paths.bios, ctx.paths.cache, ctx.game.rom.parent_path() });
		if (!program)
			throw LaunchError(ErrorCode::EmulatorMissing, "RPCS3 is not installed: install it from the emulator catalog or with your package manager.");

		YamlFile yaml = YamlFile::load(config / "config.yml");
		writeSettings(ctx, yaml);

		util::IniFile gui = util::IniFile::load(config / "GuiConfigs" / "CurrentSettings.ini");
		gui.set("main_window", "confirmationBoxExitGame", "false");
		gui.set("main_window", "infoBoxEnabledInstallPUP", "false");
		gui.set("main_window", "infoBoxEnabledWelcome", "false");

		try
		{
			fs::create_directories(hdd0(ctx));
			yaml.save(config / "config.yml");
			vfsConfig(ctx).save(config / "vfs.yml");
			inputConfig(ctx).save(config / "input_configs" / "global" / "Default.yml");
			gui.save(config / "GuiConfigs" / "CurrentSettings.ini");
		}
		catch (const fs::filesystem_error& e)
		{
			throw LaunchError(ErrorCode::InvalidConfiguration, std::string("Cannot write the RPCS3 configuration: ") + e.what());
		}

		Command cmd;
		cmd.argv = *program;
		cmd.env["XDG_CONFIG_HOME"] = home.string();
		cmd.env["XDG_CACHE_HOME"] = ctx.paths.cache.string();
		cmd.env["LC_ALL"] = "C";
		cmd.env["SDL_GAMECONTROLLERCONFIG"] = util::gameControllerConfig(ctx.controllers);
		util::forwardEnvironment(cmd);

		if (!installed)
		{
			// without dialogs; RPCS3 then aborts on its way out, whatever the result
			std::vector<std::string> install = cmd.argv;
			install.insert(install.end(), { "--headless", "--installfw", firmware->string() });
			cmd.before.push_back(std::move(install));
		}

		cmd.argv.push_back(boot.string());
		if (!ctx.settings.getBool("rpcs3_gui", false))
			cmd.argv.push_back("--no-gui");

		cmd.closeOnHotkey = true;   // no pad hotkeys of its own
		return cmd;
	}
}
