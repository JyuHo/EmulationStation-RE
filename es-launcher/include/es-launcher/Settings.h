// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.

#pragma once

#include "es-launcher/Export.h"

#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace es::launcher
{
	// The options the player chose for this launch, already split from the "global.", "<system>."
	// and "<system>[\"<game>\"]." prefixes of the ES system configuration.
	// A lookup returns the game value, else the system value, else the global value.
	// "auto" and an empty value both mean "not set": the generator picks its own default.
	class ES_LAUNCHER_API Settings
	{
	public:
		using Layer = std::map<std::string, std::string, std::less<>>;

		Settings() = default;
		Settings(Layer global, Layer system, Layer game);

		std::optional<std::string> get(std::string_view key) const;
		std::string getString(std::string_view key, std::string_view fallback = {}) const;
		bool getBool(std::string_view key, bool fallback = false) const;
		int getInt(std::string_view key, int fallback = 0) const;

		bool isSet(std::string_view key) const { return get(key).has_value(); }

		const Layer& global() const { return mGlobal; }
		const Layer& system() const { return mSystem; }
		const Layer& game() const { return mGame; }

	private:
		Layer mGlobal;
		Layer mSystem;
		Layer mGame;
	};
}
