// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.

#pragma once

#include "es-launcher/LaunchContext.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace tests
{
	namespace fs = std::filesystem;
	using namespace es::launcher;

	// A temporary directory removed at the end of the test
	struct TempDir
	{
		fs::path path;

		TempDir()
		{
			std::string tmpl = (fs::temp_directory_path() / "es-launcher-test-XXXXXX").string();
			path = mkdtemp(tmpl.data());
		}
		~TempDir()
		{
			std::error_code ec;
			fs::remove_all(path, ec);
		}
	};

	inline void writeFile(const fs::path& path, const std::string& content, bool executable = false)
	{
		fs::create_directories(path.parent_path());
		std::ofstream(path) << content;
		if (executable)
			fs::permissions(path, fs::perms::owner_all);
	}

	inline std::string readFile(const fs::path& path)
	{
		std::ifstream in(path);
		return std::string(std::istreambuf_iterator<char>(in), {});
	}

	inline Controller makePad(int player, int index)
	{
		Controller pad;
		pad.player = player;
		pad.deviceIndex = index;
		pad.guid = "030000005e0400008e02000010010000";
		pad.name = "Xbox 360 Controller";
		pad.inputs = {
			{ "a", { InputBinding::Type::Button, 0, 1 } },
			{ "b", { InputBinding::Type::Button, 1, 1 } },
			{ "x", { InputBinding::Type::Button, 2, 1 } },
			{ "y", { InputBinding::Type::Button, 3, 1 } },
			{ "pageup", { InputBinding::Type::Button, 4, 1 } },
			{ "pagedown", { InputBinding::Type::Button, 5, 1 } },
			{ "l2", { InputBinding::Type::Axis, 2, 1 } },
			{ "r2", { InputBinding::Type::Axis, 5, 1 } },
			{ "select", { InputBinding::Type::Button, 6, 1 } },
			{ "start", { InputBinding::Type::Button, 7, 1 } },
			{ "hotkey", { InputBinding::Type::Button, 8, 1 } },
			{ "up", { InputBinding::Type::Hat, 0, 1 } },
			{ "down", { InputBinding::Type::Hat, 0, 4 } },
			{ "left", { InputBinding::Type::Hat, 0, 8 } },
			{ "right", { InputBinding::Type::Hat, 0, 2 } },
			{ "joystick1up", { InputBinding::Type::Axis, 1, -1 } },
			{ "joystick1left", { InputBinding::Type::Axis, 0, -1 } },
		};
		return pad;
	}

	// Restores PATH when the test ends
	struct PathOverride
	{
		std::string old = std::getenv("PATH") ? std::getenv("PATH") : "";
		explicit PathOverride(const fs::path& dir) { setenv("PATH", dir.c_str(), 1); }
		~PathOverride() { setenv("PATH", old.c_str(), 1); }
	};
}
