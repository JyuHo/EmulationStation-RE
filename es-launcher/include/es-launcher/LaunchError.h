// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.

#pragma once

#include "es-launcher/Export.h"

#include <stdexcept>
#include <string>

namespace es::launcher
{
	// The exit codes ES already knows how to explain to the player (FileData::getMessageFromExitCode).
	enum class ErrorCode : int
	{
		EmulatorCrashed = 200,    // THE EMULATOR EXITED UNEXPECTEDLY
		BadArguments = 201,       // BAD COMMAND LINE ARGUMENTS
		InvalidConfiguration = 202,
		UnknownEmulator = 203,
		EmulatorMissing = 204,    // the emulator executable is not installed
		CoreMissing = 205,        // the libretro core is not installed
		Generic = 250,            // the message itself is shown to the player
	};

	// Thrown by a generator when the game cannot be started. what() is the message for the player.
	class ES_LAUNCHER_API LaunchError : public std::runtime_error
	{
	public:
		LaunchError(ErrorCode code, const std::string& message)
			: std::runtime_error(message), mCode(code) { }

		ErrorCode code() const noexcept { return mCode; }
		int exitCode() const noexcept { return static_cast<int>(mCode); }

	private:
		ErrorCode mCode;
	};
}
