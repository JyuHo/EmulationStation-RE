// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.

#include "util/IniFile.h"

#include "util/Files.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace es::launcher::util
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

	IniFile IniFile::load(const fs::path& path)
	{
		std::ifstream in(path, std::ios::binary);
		std::string text((std::istreambuf_iterator<char>(in)), {});
		return parse(text);
	}

	IniFile IniFile::parse(std::string_view text)
	{
		IniFile ini;

		// PPSSPP writes a UTF-8 byte order mark
		if (text.starts_with("\xEF\xBB\xBF"))
			text.remove_prefix(3);

		Section* current = nullptr;
		while (!text.empty())
		{
			size_t end = text.find('\n');
			std::string_view raw = text.substr(0, end);
			text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);

			std::string_view line = trim(raw);
			if (line.size() >= 2 && line.front() == '[' && line.back() == ']')
			{
				current = &ini.getOrAdd(trim(line.substr(1, line.size() - 2)));
				continue;
			}

			if (current == nullptr)
				current = &ini.getOrAdd("");

			size_t equal = line.find('=');
			bool comment = line.empty() || line.front() == '#' || line.front() == ';';
			if (comment || equal == std::string_view::npos || trim(line.substr(0, equal)).empty())
			{
				current->lines.push_back({ {}, std::string(trim(raw)) });
				continue;
			}

			std::string key(trim(line.substr(0, equal)));
			std::string value(trim(line.substr(equal + 1)));

			// a key written twice keeps its last value, like the emulators read it
			auto it = std::ranges::find(current->lines, key, &Line::key);
			if (it != current->lines.end())
				it->value = std::move(value);
			else
				current->lines.push_back({ std::move(key), std::move(value) });
		}

		return ini;
	}

	IniFile::Section* IniFile::find(std::string_view section)
	{
		auto it = std::ranges::find(mSections, section, &Section::name);
		return it == mSections.end() ? nullptr : &*it;
	}

	const IniFile::Section* IniFile::find(std::string_view section) const
	{
		auto it = std::ranges::find(mSections, section, &Section::name);
		return it == mSections.end() ? nullptr : &*it;
	}

	IniFile::Section& IniFile::getOrAdd(std::string_view section)
	{
		if (Section* existing = find(section))
			return *existing;

		mSections.push_back({ std::string(section), {} });
		return mSections.back();
	}

	std::optional<std::string> IniFile::get(std::string_view section, std::string_view key) const
	{
		const Section* s = find(section);
		if (s == nullptr || key.empty())
			return std::nullopt;

		auto it = std::ranges::find(s->lines, key, &Line::key);
		if (it == s->lines.end())
			return std::nullopt;
		return it->value;
	}

	bool IniFile::hasSection(std::string_view section) const
	{
		return find(section) != nullptr;
	}

	void IniFile::addSection(std::string_view section)
	{
		getOrAdd(section);
	}

	void IniFile::set(std::string_view section, std::string_view key, std::string_view value)
	{
		if (key.empty())
			return;

		Section& s = getOrAdd(section);
		auto it = std::ranges::find(s.lines, key, &Line::key);
		if (it != s.lines.end())
		{
			it->value = value;
			return;
		}

		// after the last key, before the blank lines that separate it from the next section
		auto last = std::ranges::find_if(s.lines.rbegin(), s.lines.rend(), [](const Line& l) { return !l.key.empty(); });
		s.lines.insert(last.base(), { std::string(key), std::string(value) });
	}

	void IniFile::remove(std::string_view section, std::string_view key)
	{
		if (Section* s = find(section))
			std::erase_if(s->lines, [&](const Line& l) { return !l.key.empty() && l.key == key; });
	}

	void IniFile::removeSection(std::string_view section)
	{
		std::erase_if(mSections, [&](const Section& s) { return s.name == section; });
	}

	std::string IniFile::toString() const
	{
		std::ostringstream out;
		bool empty = true;
		bool endsWithBlank = false;
		for (const Section& s : mSections)
		{
			if (!s.name.empty())
			{
				// a blank line between sections, unless the previous one already ends with one
				if (!empty && !endsWithBlank)
					out << '\n';
				out << '[' << s.name << "]\n";
				empty = false;
				endsWithBlank = false;
			}

			for (const Line& l : s.lines)
			{
				if (l.key.empty())
					out << l.value << '\n';
				else
					out << l.key << " = " << l.value << '\n';
				empty = false;
				endsWithBlank = l.key.empty() && l.value.empty();
			}
		}
		return out.str();
	}

	void IniFile::save(const fs::path& path) const
	{
		writeFileAtomically(path, toString());
	}

	std::string tomlString(std::string_view value)
	{
		std::string quoted = "\"";
		for (char c : value)
		{
			if (c == '"' || c == '\\')
				quoted += '\\';
			quoted += c;
		}
		return quoted + "\"";
	}
}
