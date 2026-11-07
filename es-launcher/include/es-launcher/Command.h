// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.

#pragma once

#include "es-launcher/Export.h"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace es::launcher
{
	// What a generator returns: the emulator process to start, once its configuration files are written.
	struct ES_LAUNCHER_API Command
	{
		// argv[0] is the executable, either an absolute path or a name looked up in PATH.
		std::vector<std::string> argv;

		// Variables added to (or overriding) the environment ES was started with.
		std::map<std::string, std::string> env;

		// Empty: the emulator starts in the directory ES runs in.
		std::filesystem::path workingDirectory;

		// The emulator cannot bind its own exit to a pad (Azahar, Cemu, xemu, Xenia): ES closes it
		// when a player holds the hotkey and presses Start. Generators of emulators that quit on
		// hotkey + Start by themselves leave it off, so ES never cuts them short.
		bool closeOnHotkey = false;

		bool empty() const { return argv.empty(); }

		// The command as one POSIX shell line ("cd 'dir' && VAR='value' 'argv0' 'arg1' ..."), every
		// word single-quoted, for code that still starts processes through a shell.
		std::string toShellLine() const;
	};

	// Single-quotes a word for a POSIX shell: abc'd becomes 'abc'\''d'.
	ES_LAUNCHER_API std::string shellQuote(const std::string& word);
}
