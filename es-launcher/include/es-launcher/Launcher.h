// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.

#pragma once

#include "es-launcher/Command.h"
#include "es-launcher/Export.h"
#include "es-launcher/Generator.h"
#include "es-launcher/LaunchContext.h"
#include "es-launcher/LaunchError.h"

#include <memory>

namespace es::launcher
{
	// One game launch, from configuration to cleanup. This is what ES calls:
	//
	//   es::launcher::Launch launch(ctx);
	//   if (!launch.handled())
	//       ... no generator for ctx.emulator: run the es_systems.cfg command as before ...
	//   Command cmd = launch.prepare();   // may throw LaunchError
	//   ... start cmd, wait for it ...
	//   launch.finish();
	class ES_LAUNCHER_API Launch
	{
	public:
		explicit Launch(LaunchContext ctx);
		~Launch();

		Launch(const Launch&) = delete;
		Launch& operator=(const Launch&) = delete;

		// True when a generator is registered for ctx.emulator.
		bool handled() const { return mGenerator != nullptr; }

		const LaunchContext& context() const { return mContext; }

		// Writes the emulator configuration and returns the command to run.
		// Throws LaunchError (UnknownEmulator when !handled()).
		Command prepare();

		// Runs the generator's cleanup once. Also called by the destructor if prepare() succeeded.
		void finish();

	private:
		LaunchContext mContext;
		std::unique_ptr<Generator> mGenerator;
		bool mPrepared = false;
	};
}
