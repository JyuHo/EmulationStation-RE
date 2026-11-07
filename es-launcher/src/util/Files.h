// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace es::launcher::util
{
	// The executable called name in $PATH, or name itself when it is a path to an executable file.
	std::optional<std::filesystem::path> findExecutable(std::string_view name);

	// The first directory of dirs where fileName exists.
	std::optional<std::filesystem::path> findFile(std::string_view fileName, const std::vector<std::filesystem::path>& dirs);

	// The directories listed in a colon separated environment variable, if it is set.
	std::vector<std::filesystem::path> pathsFromEnv(const char* variable);

	// "/roms/psx/Final Fantasy VII (Disc 1).chd" -> "Final Fantasy VII (Disc 1).chd"
	std::string fileName(const std::filesystem::path& path);

	// Writes next to the target, then renames, so a crash never leaves half a file. Creates the
	// parent directory. Throws std::filesystem::filesystem_error on failure.
	void writeFileAtomically(const std::filesystem::path& path, std::string_view content);

	// Makes `link` a symbolic link to the directory `target` (created if needed), so an emulator
	// that keeps its saves inside its own folder writes them where ES keeps saves. Does nothing
	// when `link` already exists, as a real folder or a link: the player's choice is kept.
	// Returns false when the link could not be made.
	bool linkDirectory(const std::filesystem::path& link, const std::filesystem::path& target);
}
