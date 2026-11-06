// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.

#include "es-launcher/Generator.h"

#include <algorithm>

namespace es::launcher
{
	Registry& Registry::instance()
	{
		static Registry registry;
		return registry;
	}

	bool Registry::add(std::string emulator, Factory factory)
	{
		if (emulator.empty() || !factory || contains(emulator))
			return false;

		mEntries.push_back({ std::move(emulator), std::move(factory) });
		return true;
	}

	bool Registry::contains(std::string_view emulator) const
	{
		return std::ranges::any_of(mEntries, [&](const Entry& e) { return e.emulator == emulator; });
	}

	std::unique_ptr<Generator> Registry::create(std::string_view emulator) const
	{
		auto it = std::ranges::find(mEntries, emulator, &Entry::emulator);
		return it == mEntries.end() ? nullptr : it->factory();
	}

	std::vector<std::string> Registry::names() const
	{
		std::vector<std::string> result;
		result.reserve(mEntries.size());
		for (const Entry& e : mEntries)
			result.push_back(e.emulator);
		std::ranges::sort(result);
		return result;
	}
}
