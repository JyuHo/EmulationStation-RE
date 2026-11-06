// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.

#include "es-launcher/LaunchContext.h"

namespace es::launcher
{
	Architecture currentArchitecture()
	{
#if defined(__x86_64__)
		return Architecture::X86_64;
#elif defined(__aarch64__)
		return Architecture::AArch64;
#else
		return Architecture::Other;
#endif
	}

	const InputBinding* Controller::find(std::string_view input) const
	{
		auto it = inputs.find(input);
		return it == inputs.end() ? nullptr : &it->second;
	}
}
