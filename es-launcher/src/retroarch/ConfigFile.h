// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (LibretroConfig), Copyright (c) 2026 The Batocera Team.

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace es::launcher::retroarch
{
	// A RetroArch configuration file (retroarch.cfg, core options): lines of `key = "value"`.
	// Keys keep the order they were read in, new keys are appended, so the file stays readable
	// and the changes the player made from the RetroArch menu survive between launches.
	class ConfigFile
	{
	public:
		// An unreadable or missing file gives an empty configuration.
		static ConfigFile load(const std::filesystem::path& path);

		std::optional<std::string> get(std::string_view key) const;

		void set(std::string_view key, std::string_view value);
		void set(std::string_view key, const char* value) { set(key, std::string_view(value)); }
		void set(std::string_view key, const std::string& value) { set(key, std::string_view(value)); }
		void set(std::string_view key, bool value) { set(key, value ? "true" : "false"); }
		void set(std::string_view key, int value) { set(key, std::to_string(value)); }

		void remove(std::string_view key);
		void removeStartingWith(std::string_view prefix);

		// Creates the parent directory. Throws std::filesystem::filesystem_error on failure.
		void save(const std::filesystem::path& path) const;

		std::string toString() const;
		size_t size() const { return mEntries.size(); }

	private:
		struct Entry
		{
			std::string key;
			std::string value;
		};
		std::vector<Entry> mEntries;
	};
}
