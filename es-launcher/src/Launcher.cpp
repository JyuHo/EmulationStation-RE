// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.

#include "es-launcher/Launcher.h"

namespace es::launcher
{
	Launch::Launch(LaunchContext ctx)
		: mContext(std::move(ctx)), mGenerator(Registry::instance().create(mContext.emulator))
	{
	}

	Launch::~Launch()
	{
		try
		{
			finish();
		}
		catch (...)
		{
			// a failing cleanup must not take ES down with it
		}
	}

	Command Launch::prepare()
	{
		if (!mGenerator)
			throw LaunchError(ErrorCode::UnknownEmulator, "No generator for emulator '" + mContext.emulator + "'");

		Command command = mGenerator->configure(mContext);
		if (command.empty())
			throw LaunchError(ErrorCode::InvalidConfiguration, "The '" + mContext.emulator + "' generator returned no command");

		mPrepared = true;
		return command;
	}

	void Launch::finish()
	{
		if (!mPrepared || !mGenerator)
			return;

		mPrepared = false;
		mGenerator->cleanup(mContext);
	}
}
