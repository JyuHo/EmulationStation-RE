// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.

#pragma once

#include "es-launcher/Command.h"
#include "es-launcher/Export.h"
#include "es-launcher/LaunchContext.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace es::launcher
{
	// Configures one emulator for one game. A new instance is created for every launch.
	//
	// configure() writes the emulator's configuration files (in ctx.paths.configFor(...)) and returns
	// the command to start it. It throws LaunchError when the game cannot be started (missing BIOS,
	// emulator not installed...). cleanup() runs after the emulator has exited, even if it failed.
	class ES_LAUNCHER_API Generator
	{
	public:
		virtual ~Generator() = default;

		virtual Command configure(const LaunchContext& ctx) = 0;
		virtual void cleanup(const LaunchContext& /*ctx*/) { }
	};

	// Generators by emulator name, as written in the <emulator name="..."> of es_systems.cfg.
	class ES_LAUNCHER_API Registry
	{
	public:
		using Factory = std::function<std::unique_ptr<Generator>()>;

		static Registry& instance();

		// Returns false (and keeps the first one) if a generator is already registered under this name.
		bool add(std::string emulator, Factory factory);

		bool contains(std::string_view emulator) const;
		std::unique_ptr<Generator> create(std::string_view emulator) const;
		std::vector<std::string> names() const;

	private:
		Registry() = default;

		struct Entry
		{
			std::string emulator;
			Factory factory;
		};
		std::vector<Entry> mEntries;
	};

	// Registers a generator when the library loads:
	//   ES_LAUNCHER_REGISTER_GENERATOR("dolphin", DolphinGenerator)
	#define ES_LAUNCHER_REGISTER_GENERATOR(name, Type)                                   \
		namespace                                                                          \
		{                                                                                  \
			const bool registered_##Type = ::es::launcher::Registry::instance().add(       \
				name, [] { return std::unique_ptr<::es::launcher::Generator>(new Type()); }); \
		}
}
