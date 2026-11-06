// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.

#pragma once

#include <cstdio>

namespace tests
{
	inline int failures = 0;

	inline void check(bool condition, const char* expression, const char* file, int line)
	{
		if (!condition)
		{
			std::fprintf(stderr, "%s:%d: CHECK(%s) failed\n", file, line, expression);
			++failures;
		}
	}

	void runLauncherTests();
	void runRetroArchTests();
}

#define CHECK(x) ::tests::check((x), #x, __FILE__, __LINE__)
