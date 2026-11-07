// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.

#include "util/EmulatorCommand.h"

#include "util/Files.h"

#include <algorithm>
#include <set>

namespace es::launcher::util
{
	namespace fs = std::filesystem;

	static bool isFlatpakRun(const std::vector<std::string>& argv)
	{
		return argv.size() >= 3 && fs::path(argv[0]).filename() == "flatpak" && argv[1] == "run";
	}

	std::optional<std::vector<std::string>> emulatorCommand(const LaunchContext& ctx, std::string_view id, std::string_view binary,
		const std::vector<fs::path>& sharedPaths)
	{
		auto installed = ctx.installedEmulators.find(id);
		if (installed != ctx.installedEmulators.end() && !installed->second.empty())
		{
			std::vector<std::string> argv = installed->second;

			// The program must still be there: the player may have removed it by hand
			if (auto program = findExecutable(argv[0]))
			{
				argv[0] = program->string();

				if (isFlatpakRun(argv))
				{
					std::set<std::string> seen;
					std::vector<std::string> options;
					for (const auto& path : sharedPaths)
					{
						if (path.empty())
							continue;

						std::string option = "--filesystem=" + path.lexically_normal().string();
						if (seen.insert(option).second)
							options.push_back(option);
					}

					argv.insert(argv.begin() + 2, options.begin(), options.end());
				}

				return argv;
			}
		}

		if (auto program = findExecutable(binary))
			return std::vector<std::string>{ program->string() };

		return std::nullopt;
	}

	std::optional<std::vector<std::string>> emulatorCommand(const LaunchContext& ctx, std::string_view id,
		std::initializer_list<std::string_view> binaries, const std::vector<fs::path>& sharedPaths)
	{
		std::optional<std::vector<std::string>> found;
		for (std::string_view binary : binaries)
		{
			// the catalog is looked up on each call: only the PATH fallback differs
			found = emulatorCommand(ctx, id, binary, sharedPaths);
			if (found)
				return found;
		}
		if (binaries.size() == 0)
			found = emulatorCommand(ctx, id, std::string_view(), sharedPaths);
		return found;
	}

	void forwardEnvironment(Command& cmd)
	{
		if (!isFlatpakRun(cmd.argv))
			return;

		// the application id is the first word after "run" that is not an option
		auto app = std::find_if(cmd.argv.begin() + 2, cmd.argv.end(), [](const std::string& word) { return !word.starts_with("-"); });

		std::vector<std::string> options;
		for (const auto& [name, value] : cmd.env)
			options.push_back("--env=" + name + "=" + value);

		cmd.argv.insert(app, options.begin(), options.end());
	}
}
