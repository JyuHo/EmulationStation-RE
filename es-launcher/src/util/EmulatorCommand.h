// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.

#pragma once

#include "es-launcher/Command.h"
#include "es-launcher/LaunchContext.h"

#include <filesystem>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace es::launcher::util
{
	// The command that starts an emulator, without its arguments: generators append theirs.
	//
	//   1. what ES's emulator catalog installed under `id` (ctx.installedEmulators), when its
	//      program is still there;
	//   2. else `binary` found in PATH.
	//
	// A Flatpak runs sandboxed: it is given read-write access to each of `sharedPaths` (its
	// configuration, the saves, the BIOS, the game...), inserted as --filesystem= options.
	std::optional<std::vector<std::string>> emulatorCommand(const LaunchContext& ctx, std::string_view id, std::string_view binary,
		const std::vector<std::filesystem::path>& sharedPaths = {});

	// The same, trying several program names in PATH, in order: distributions do not agree on
	// them ("PPSSPPSDL", "ppsspp").
	std::optional<std::vector<std::string>> emulatorCommand(const LaunchContext& ctx, std::string_view id,
		std::initializer_list<std::string_view> binaries, const std::vector<std::filesystem::path>& sharedPaths = {});

	// A Flatpak does not see the environment of the process that starts it (its XDG_* variables
	// are its own): copies cmd.env into --env= options, before the application id. Does nothing
	// for any other command. Call it once the environment is complete.
	void forwardEnvironment(Command& cmd);
}
