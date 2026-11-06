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
}
