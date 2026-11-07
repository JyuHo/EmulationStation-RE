// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.

#include "es-launcher/Command.h"

namespace es::launcher
{
	std::string shellQuote(const std::string& word)
	{
		std::string quoted = "'";
		for (char c : word)
		{
			if (c == '\'')
				quoted += "'\\''";
			else
				quoted += c;
		}
		quoted += '\'';
		return quoted;
	}

	static std::string words(const std::map<std::string, std::string>& env, const std::vector<std::string>& argv)
	{
		std::string line;
		for (const auto& [name, value] : env)
			line += name + "=" + shellQuote(value) + " ";

		for (size_t i = 0; i < argv.size(); ++i)
		{
			if (i > 0)
				line += ' ';
			line += shellQuote(argv[i]);
		}
		return line;
	}

	std::string Command::toShellLine() const
	{
		std::string line;
		for (const auto& step : before)
			line += words(env, step) + " ; ";
		line += words(env, argv);

		if (!before.empty())   // one command for the redirections ES adds after it
			line = "{ " + line + " ; }";

		if (!workingDirectory.empty())
			line = "cd " + shellQuote(workingDirectory.string()) + " && " + line;

		return line;
	}
}
