// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (LibretroConfig), Copyright (c) 2026 The Batocera Team.

#include "retroarch/ConfigFile.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace es::launcher::retroarch
{
	namespace fs = std::filesystem;

	static std::string_view trim(std::string_view s)
	{
		while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
			s.remove_prefix(1);
		while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
			s.remove_suffix(1);
		return s;
	}

	ConfigFile ConfigFile::load(const fs::path& path)
	{
		ConfigFile config;

		std::ifstream in(path);
		std::string line;
		while (std::getline(in, line))
		{
			std::string_view view = trim(line);
			if (view.empty() || view.front() == '#')
				continue;

			size_t equal = view.find('=');
			if (equal == std::string_view::npos)
				continue;

			std::string_view key = trim(view.substr(0, equal));
			std::string_view value = trim(view.substr(equal + 1));
			if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
				value = value.substr(1, value.size() - 2);

			if (!key.empty())
				config.set(key, value);
		}

		return config;
	}

	std::optional<std::string> ConfigFile::get(std::string_view key) const
	{
		auto it = std::ranges::find(mEntries, key, &Entry::key);
		if (it == mEntries.end())
			return std::nullopt;
		return it->value;
	}

	void ConfigFile::set(std::string_view key, std::string_view value)
	{
		auto it = std::ranges::find(mEntries, key, &Entry::key);
		if (it != mEntries.end())
			it->value = value;
		else
			mEntries.push_back({ std::string(key), std::string(value) });
	}

	void ConfigFile::remove(std::string_view key)
	{
		std::erase_if(mEntries, [&](const Entry& e) { return e.key == key; });
	}

	void ConfigFile::removeStartingWith(std::string_view prefix)
	{
		std::erase_if(mEntries, [&](const Entry& e) { return e.key.starts_with(prefix); });
	}

	std::string ConfigFile::toString() const
	{
		std::ostringstream out;
		for (const Entry& e : mEntries)
			out << e.key << " = \"" << e.value << "\"\n";
		return out.str();
	}

	void ConfigFile::save(const fs::path& path) const
	{
		if (path.has_parent_path())
			fs::create_directories(path.parent_path());

		// write next to the target, then rename, so a crash never leaves half a file
		fs::path tmp = path;
		tmp += ".tmp";
		{
			std::ofstream out(tmp, std::ios::trunc);
			if (!out)
				throw fs::filesystem_error("cannot write", tmp, std::make_error_code(std::errc::io_error));
			out << toString();
		}
		fs::rename(tmp, path);
	}
}
