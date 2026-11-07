// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.cemu), Copyright (c) 2026 The Batocera Team.

#include "cemu/CemuGenerator.h"

#include "es-launcher/LaunchError.h"
#include "util/EmulatorCommand.h"
#include "util/Files.h"
#include "util/Gamepad.h"

#include <pugixml.hpp>

#include <algorithm>
#include <cstdlib>
#include <span>
#include <sstream>
#include <utility>

namespace es::launcher::cemu
{
	namespace fs = std::filesystem;

	ES_LAUNCHER_REGISTER_GENERATOR("cemu", CemuGenerator)

	namespace
	{
		constexpr const char* gamePad = "Wii U GamePad";
		constexpr const char* proController = "Wii U Pro Controller";
		constexpr const char* classicController = "Wii U Classic Controller";
		constexpr const char* wiimote = "Wiimote";

		using Mapping = std::pair<int, int>;   // Cemu's emulated button -> SDL gamepad button of Cemu's SDLController

		// The mappings of batocera-launch
		constexpr Mapping gamePadMapping[] = {
			{ 1, 1 }, { 2, 0 }, { 3, 3 }, { 4, 2 }, { 5, 9 }, { 6, 10 }, { 7, 42 }, { 8, 43 }, { 9, 6 }, { 10, 4 },
			{ 11, 11 }, { 12, 12 }, { 13, 13 }, { 14, 14 }, { 15, 7 }, { 16, 8 }, { 17, 45 }, { 18, 39 }, { 19, 44 },
			{ 20, 38 }, { 21, 47 }, { 22, 41 }, { 23, 46 }, { 24, 40 }, { 25, 7 },
		};
		constexpr Mapping proMapping[] = {
			{ 1, 1 }, { 2, 0 }, { 3, 3 }, { 4, 2 }, { 5, 9 }, { 6, 10 }, { 7, 42 }, { 8, 43 }, { 9, 6 }, { 10, 4 },
			{ 12, 11 }, { 13, 12 }, { 14, 13 }, { 15, 14 }, { 16, 7 }, { 17, 8 }, { 18, 45 }, { 19, 39 }, { 20, 44 },
			{ 21, 38 }, { 22, 47 }, { 23, 41 }, { 24, 46 }, { 25, 40 },
		};
		constexpr Mapping classicMapping[] = {
			{ 1, 13 }, { 2, 12 }, { 3, 15 }, { 4, 14 }, { 5, 8 }, { 6, 9 }, { 7, 42 }, { 8, 43 }, { 9, 4 }, { 10, 5 },
			{ 12, 0 }, { 13, 1 }, { 14, 2 }, { 15, 3 }, { 16, 39 }, { 17, 45 }, { 18, 44 }, { 19, 38 }, { 20, 41 },
			{ 21, 47 }, { 22, 46 }, { 23, 40 },
		};
		constexpr Mapping wiimoteMapping[] = {
			{ 1, 0 }, { 2, 43 }, { 3, 2 }, { 4, 1 }, { 5, 42 }, { 6, 9 }, { 7, 6 }, { 8, 4 }, { 9, 11 }, { 10, 12 },
			{ 11, 13 }, { 12, 14 }, { 13, 45 }, { 14, 39 }, { 15, 44 }, { 16, 38 },
		};

		constexpr std::pair<const char*, int> languages[] = {
			{ "ja_JP", 0 }, { "en_US", 1 }, { "fr_FR", 2 }, { "de_DE", 3 }, { "it_IT", 4 }, { "es_ES", 5 },
			{ "zh_CN", 6 }, { "ko_KR", 7 }, { "nl_NL", 8 }, { "pt_PT", 9 }, { "ru_RU", 10 }, { "zh_TW", 11 },
		};

		int language(std::string locale)
		{
			if (locale.empty() || locale == "ui")
			{
				const char* lang = std::getenv("LANG");
				locale = lang != nullptr ? std::string(std::string_view(lang).substr(0, 5)) : "en_US";
			}
			for (const auto& [code, value] : languages)
				if (locale == code)
					return value;
			return 1;
		}

		// The child called name, created when missing
		pugi::xml_node child(pugi::xml_node parent, const char* name)
		{
			pugi::xml_node node = parent.child(name);
			return node ? node : parent.append_child(name);
		}

		void setValue(pugi::xml_node parent, const char* name, const std::string& value)
		{
			child(parent, name).text().set(value.c_str());
		}

		const char* boolString(bool value) { return value ? "true" : "false"; }

		std::string controllerType(const LaunchContext& ctx, size_t player)
		{
			std::string combination = ctx.settings.getString("cemu_controller_combination", "0");
			if (combination == "1")
				return player == 0 ? gamePad : wiimote;
			if (combination == "2")
				return proController;
			if (combination == "3")
				return wiimote;
			if (combination == "4")
				return classicController;
			return player == 0 ? gamePad : proController;
		}
	}

	fs::path CemuGenerator::configDirectory(const LaunchContext& ctx)
	{
		return ctx.paths.configFor("cemu") / "Cemu";
	}

	void CemuGenerator::writeSettings(const LaunchContext& ctx, pugi::xml_document& doc)
	{
		const Settings& s = ctx.settings;
		pugi::xml_node root = child(doc, "content");

		setValue(root, "mlc_path", (ctx.paths.saves / "wiiu" / "mlc01").string());
		setValue(root, "check_update", "false");
		setValue(root, "gp_download", "true");   // no welcome window
		setValue(root, "logflag", "0");
		setValue(root, "advanced_ppc_logging", "false");
		setValue(root, "use_discord_presence", "false");
		setValue(root, "fullscreen_menubar", "false");
		setValue(root, "vk_warning", "false");
		setValue(root, "fullscreen", "true");
		setValue(root, "console_language", std::to_string(language(s.getString("cemu_console_language", "ui"))));
		setValue(root, "open_pad", boolString(s.getBool("cemu_gamepad", false)));

		pugi::xml_node paths = child(root, "GamePaths");
		std::string romDir = ctx.game.rom.parent_path().string();
		bool listed = false;
		for (pugi::xml_node entry : paths.children("Entry"))
			listed = listed || romDir == entry.text().get();
		if (!listed)
			paths.append_child("Entry").text().set(romDir.c_str());

		// [Graphic]: 0 OpenGL, 1 Vulkan
		pugi::xml_node graphic = child(root, "Graphic");
		setValue(graphic, "api", s.getString("cemu_gfxbackend", "1"));
		setValue(graphic, "AsyncCompile", boolString(s.getBool("cemu_async", true)));
		setValue(graphic, "GX2DrawdoneSync", boolString(s.getBool("cemu_gx2drawdone", false)));
		setValue(graphic, "VSync", s.getString("cemu_vsync", "1"));
		setValue(graphic, "UpscaleFilter", s.getString("cemu_upscale", "2"));
		setValue(graphic, "DownscaleFilter", s.getString("cemu_downscale", "0"));
		setValue(graphic, "FullscreenScaling", s.getString("cemu_aspect", "0"));

		bool overlay = s.getBool("cemu_overlay", false);
		pugi::xml_node overlayNode = child(graphic, "Overlay");
		setValue(overlayNode, "Position", "3");
		for (const char* key : { "FPS", "DrawCalls", "CPUUsage", "CPUPerCoreUsage", "RAMUsage", "VRAMUsage" })
			setValue(overlayNode, key, boolString(overlay));

		bool notifications = s.getBool("cemu_notifications", false);
		pugi::xml_node notificationNode = child(graphic, "Notification");
		setValue(notificationNode, "Position", "1");
		for (const char* key : { "ControllerProfiles", "ControllerBattery", "ShaderCompiling", "FriendService" })
			setValue(notificationNode, key, boolString(notifications));

		// [Audio]: cubeb, the TV only
		pugi::xml_node audio = child(root, "Audio");
		setValue(audio, "api", "3");
		setValue(audio, "TVChannels", s.getString("cemu_audio_channels", "1"));
		setValue(audio, "TVVolume", "100");
	}

	std::string CemuGenerator::controllerProfile(const LaunchContext& ctx, size_t player)
	{
		const Controller& pad = ctx.controllers.at(player);

		// Cemu finds a pad by GUID, numbered among those of the same GUID in device order
		auto before = std::ranges::count_if(ctx.controllers, [&](const Controller& other) {
			return other.guid == pad.guid && other.deviceIndex < pad.deviceIndex;
		});

		std::string type = controllerType(ctx, player);
		std::span<const Mapping> mapping = gamePadMapping;
		if (type == proController)
			mapping = proMapping;
		else if (type == classicController)
			mapping = classicMapping;
		else if (type == wiimote)
			mapping = wiimoteMapping;

		pugi::xml_document doc;
		pugi::xml_node root = doc.append_child("emulated_controller");
		root.append_child("type").text().set(type.c_str());

		pugi::xml_node controller = root.append_child("controller");
		controller.append_child("api").text().set("SDLController");
		controller.append_child("uuid").text().set((std::to_string(before) + "_" + pad.guid).c_str());
		controller.append_child("display_name").text().set(pad.name.c_str());
		controller.append_child("rumble").text().set(ctx.settings.getString("cemu_rumble", "0").c_str());
		for (const char* analog : { "axis", "rotation", "trigger" })
		{
			pugi::xml_node node = controller.append_child(analog);
			node.append_child("deadzone").text().set("0.25");
			node.append_child("range").text().set("1");
		}

		pugi::xml_node mappings = controller.append_child("mappings");
		for (const auto& [emulated, button] : mapping)
		{
			pugi::xml_node entry = mappings.append_child("entry");
			entry.append_child("mapping").text().set(emulated);
			entry.append_child("button").text().set(button);
		}

		std::ostringstream out;
		doc.save(out, "  ", pugi::format_default | pugi::format_save_file_text, pugi::encoding_utf8);
		return out.str();
	}

	Command CemuGenerator::configure(const LaunchContext& ctx)
	{
		if (ctx.architecture != Architecture::X86_64)
			throw LaunchError(ErrorCode::Generic, "Cemu only runs on x86_64 computers.");

		fs::path home = ctx.paths.configFor("cemu");
		fs::path config = configDirectory(ctx);
		fs::path saves = ctx.paths.saves / "wiiu";

		auto program = util::emulatorCommand(ctx, "cemu", { "cemu", "Cemu" }, {
			home, saves, ctx.paths.bios, ctx.paths.cache, ctx.game.rom.parent_path() });
		if (!program)
			throw LaunchError(ErrorCode::EmulatorMissing, "Cemu is not installed: install it from the emulator catalog or with your package manager.");

		// a game folder (extracted) starts from its code/*.rpx
		fs::path rom = ctx.game.rom;
		std::error_code ec;
		if (fs::is_directory(rom, ec))
		{
			for (const auto& entry : fs::recursive_directory_iterator(rom, ec))
			{
				if (entry.path().extension() == ".rpx" && entry.path().parent_path().filename() == "code")
				{
					rom = entry.path();
					break;
				}
			}
		}

		pugi::xml_document settings;
		settings.load_file((config / "settings.xml").c_str());
		writeSettings(ctx, settings);

		try
		{
			fs::create_directories(saves / "mlc01");
			std::ostringstream out;
			settings.save(out, "  ");
			util::writeFileAtomically(config / "settings.xml", out.str());

			fs::path profiles = config / "controllerProfiles";
			for (size_t i = 0; i < 8; ++i)
				fs::remove(profiles / ("controller" + std::to_string(i) + ".xml"), ec);
			for (size_t i = 0; i < ctx.controllers.size() && i < 8; ++i)
				util::writeFileAtomically(profiles / ("controller" + std::to_string(i) + ".xml"), controllerProfile(ctx, i));

			// the keys of encrypted games, kept with the BIOS
			fs::path data = saves / "Cemu";
			fs::create_directories(data);
			if (!fs::exists(fs::symlink_status(data / "keys.txt", ec)))
			{
				for (const fs::path& keys : { ctx.paths.bios / "cemu" / "keys.txt", ctx.paths.bios / "keys.txt" })
				{
					if (fs::is_regular_file(keys, ec))
					{
						fs::create_symlink(keys, data / "keys.txt", ec);
						break;
					}
				}
			}
		}
		catch (const fs::filesystem_error& e)
		{
			throw LaunchError(ErrorCode::InvalidConfiguration, std::string("Cannot write the Cemu configuration: ") + e.what());
		}

		Command cmd;
		cmd.argv = *program;
		cmd.argv.insert(cmd.argv.end(), { "-f", "-g", rom.string() });

		cmd.env["XDG_CONFIG_HOME"] = home.string();
		cmd.env["XDG_DATA_HOME"] = saves.string();
		cmd.env["XDG_CACHE_HOME"] = ctx.paths.cache.string();
		cmd.env["SDL_GAMECONTROLLERCONFIG"] = util::gameControllerConfig(ctx.controllers);
		cmd.closeOnHotkey = true;   // no pad hotkeys of its own
		util::forwardEnvironment(cmd);
		return cmd;
	}
}
