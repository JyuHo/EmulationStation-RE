// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.

#pragma once

#include "es-launcher/Export.h"
#include "es-launcher/Settings.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace es::launcher
{
	enum class Architecture
	{
		X86_64,
		AArch64,
		Other,
	};

	// The architecture es-launcher was built for.
	ES_LAUNCHER_API Architecture currentArchitecture();

	// One control of a controller, as ES maps it (InputConfig): "a", "b", "start", "up", "joystick1left"...
	struct ES_LAUNCHER_API InputBinding
	{
		enum class Type
		{
			Button,
			Axis,
			Hat,
			Key,
		};

		Type type = Type::Button;
		int id = -1;    // button, axis or hat number, or SDL keycode
		int value = 0;  // axis direction (-1 / 1) or hat mask; 1 for buttons
	};

	// A controller assigned to a player, in the order the player chose in ES.
	struct ES_LAUNCHER_API Controller
	{
		int player = 1;           // 1-based player number
		int deviceIndex = -1;     // index of the joystick as the emulator's SDL sees it
		std::string guid;         // SDL joystick GUID
		std::string name;
		std::string devicePath;   // /dev/input/eventN
		int buttonCount = 0;
		int hatCount = 0;
		int axisCount = 0;
		std::map<std::string, InputBinding, std::less<>> inputs;

		const InputBinding* find(std::string_view input) const;
	};

	// Where things live. ES fills these from its XDG paths; generators never hard-code a location.
	struct ES_LAUNCHER_API LaunchPaths
	{
		std::filesystem::path bios;
		std::filesystem::path saves;
		std::filesystem::path screenshots;
		std::filesystem::path cache;
		std::filesystem::path runtime;

		// Each emulator gets its own folder below this one, so ES never writes into the player's
		// own emulator configuration (~/.config/dolphin-emu, ...).
		std::filesystem::path emulatorConfigs;

		std::filesystem::path configFor(std::string_view emulator) const { return emulatorConfigs / emulator; }

		// libretro cores installed by ES's emulator catalog, with their .info files in info/.
		// Looked up before the distribution's cores. Empty when ES has none.
		std::filesystem::path cores;
	};

	struct ES_LAUNCHER_API Netplay
	{
		enum class Mode
		{
			Host,
			Client,
			Spectator,
		};

		Mode mode = Mode::Host;
		std::string host;      // client and spectator only
		int port = 55435;
		std::string password;
		std::string session;   // relay (MITM) session
	};

	struct ES_LAUNCHER_API SaveState
	{
		std::optional<int> slot;
		std::filesystem::path file;   // state to load at startup, empty for none
		bool autoSave = false;
	};

	struct ES_LAUNCHER_API Game
	{
		std::filesystem::path rom;
		std::string name;             // display name from the gamelist
		std::string system;           // system id: "psx", "gamecube"...
		std::string systemName;       // full system name: "PlayStation"
		std::map<std::string, std::string, std::less<>> metadata;   // other gamelist fields
	};

	// Everything a generator needs to configure and start one game. ES builds it right before the
	// launch; a generator only reads it.
	struct ES_LAUNCHER_API LaunchContext
	{
		Game game;
		std::string emulator;         // "retroarch", "dolphin"...
		std::string core;             // libretro core, or the emulator variant; may be empty
		Settings settings;
		std::vector<Controller> controllers;
		LaunchPaths paths;

		// Emulators installed by ES's emulator catalog (installed.json): emulator id -> the
		// command that starts it, without arguments ({ ".../pcsx2.AppImage" } or
		// { "flatpak", "run", "net.pcsx2.PCSX2" }). Generators find them with
		// util::emulatorCommand(), which falls back to PATH.
		std::map<std::string, std::vector<std::string>, std::less<>> installedEmulators;

		std::optional<Netplay> netplay;
		SaveState saveState;
		Architecture architecture = currentArchitecture();

		// The display ES runs on, so the emulator can match it.
		int screenWidth = 0;
		int screenHeight = 0;
	};
}
