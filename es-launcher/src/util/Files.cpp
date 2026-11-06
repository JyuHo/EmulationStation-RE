// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.

#include "util/Files.h"

#include <cstdlib>
#include <unistd.h>

namespace es::launcher::util
{
	namespace fs = std::filesystem;

	static bool isExecutableFile(const fs::path& path)
	{
		std::error_code ec;
		return fs::is_regular_file(path, ec) && access(path.c_str(), X_OK) == 0;
	}

	std::vector<fs::path> pathsFromEnv(const char* variable)
	{
		std::vector<fs::path> result;

		const char* value = std::getenv(variable);
		if (value == nullptr)
			return result;

		std::string_view rest(value);
		while (!rest.empty())
		{
			size_t colon = rest.find(':');
			std::string_view entry = rest.substr(0, colon);
			if (!entry.empty())
				result.emplace_back(entry);

			if (colon == std::string_view::npos)
				break;
			rest.remove_prefix(colon + 1);
		}

		return result;
	}

	std::optional<fs::path> findExecutable(std::string_view name)
	{
		if (name.empty())
			return std::nullopt;

		if (name.find('/') != std::string_view::npos)
		{
			fs::path path(name);
			return isExecutableFile(path) ? std::optional(path) : std::nullopt;
		}

		for (const auto& dir : pathsFromEnv("PATH"))
		{
			fs::path candidate = dir / name;
			if (isExecutableFile(candidate))
				return candidate;
		}

		return std::nullopt;
	}

	std::optional<fs::path> findFile(std::string_view fileName, const std::vector<fs::path>& dirs)
	{
		for (const auto& dir : dirs)
		{
			std::error_code ec;
			fs::path candidate = dir / fileName;
			if (fs::is_regular_file(candidate, ec))
				return candidate;
		}

		return std::nullopt;
	}

	std::string fileName(const fs::path& path)
	{
		return path.filename().string();
	}
}
