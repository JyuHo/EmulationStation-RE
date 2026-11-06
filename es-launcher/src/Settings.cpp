// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.

#include "es-launcher/Settings.h"

#include <charconv>

namespace es::launcher
{
	namespace
	{
		std::optional<std::string> lookup(const Settings::Layer& layer, std::string_view key)
		{
			auto it = layer.find(key);
			if (it == layer.end() || it->second.empty() || it->second == "auto")
				return std::nullopt;
			return it->second;
		}
	}

	Settings::Settings(Layer global, Layer system, Layer game)
		: mGlobal(std::move(global)), mSystem(std::move(system)), mGame(std::move(game))
	{
	}

	std::optional<std::string> Settings::get(std::string_view key) const
	{
		for (const Layer* layer : { &mGame, &mSystem, &mGlobal })
		{
			if (auto value = lookup(*layer, key))
				return value;
		}
		return std::nullopt;
	}

	std::string Settings::getString(std::string_view key, std::string_view fallback) const
	{
		auto value = get(key);
		return value ? *value : std::string(fallback);
	}

	bool Settings::getBool(std::string_view key, bool fallback) const
	{
		auto value = get(key);
		if (!value)
			return fallback;
		if (*value == "1" || *value == "true" || *value == "on" || *value == "yes")
			return true;
		if (*value == "0" || *value == "false" || *value == "off" || *value == "no")
			return false;
		return fallback;
	}

	int Settings::getInt(std::string_view key, int fallback) const
	{
		auto value = get(key);
		if (!value)
			return fallback;

		int result = 0;
		const char* first = value->data();
		const char* last = first + value->size();
		auto [ptr, ec] = std::from_chars(first, last, result);
		if (ec != std::errc() || ptr != last)
			return fallback;
		return result;
	}
}
