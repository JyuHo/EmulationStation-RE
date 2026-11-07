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
	// An INI file as the standalone emulators write them (DuckStation, PCSX2, PPSSPP, Dolphin):
	// [Section] headers and `Key = Value` lines. Sections, keys and comments keep their order, so
	// what the player changed in the emulator's own menus survives between launches. Names are
	// case-sensitive, like the emulators' own parsers.
	class IniFile
	{
	public:
		// An unreadable or missing file gives an empty file.
		static IniFile load(const std::filesystem::path& path);
		static IniFile parse(std::string_view text);

		std::optional<std::string> get(std::string_view section, std::string_view key) const;
		bool hasSection(std::string_view section) const;

		// Adds the section and the key at the end when they are missing.
		void set(std::string_view section, std::string_view key, std::string_view value);
		void set(std::string_view section, std::string_view key, const char* value) { set(section, key, std::string_view(value)); }
		void set(std::string_view section, std::string_view key, const std::string& value) { set(section, key, std::string_view(value)); }
		void set(std::string_view section, std::string_view key, int value) { set(section, key, std::to_string(value)); }

		// Adds the section, empty, when it is missing.
		void addSection(std::string_view section);

		void remove(std::string_view section, std::string_view key);
		void removeSection(std::string_view section);

		// Creates the parent directory and replaces the file atomically. Throws
		// std::filesystem::filesystem_error on failure.
		void save(const std::filesystem::path& path) const;

		std::string toString() const;

	private:
		struct Line
		{
			std::string key;     // empty for a comment or a blank line
			std::string value;   // the whole line for a comment
		};

		struct Section
		{
			std::string name;    // empty for the lines before the first header
			std::vector<Line> lines;
		};

		Section* find(std::string_view section);
		const Section* find(std::string_view section) const;
		Section& getOrAdd(std::string_view section);

		std::vector<Section> mSections;
	};
}
