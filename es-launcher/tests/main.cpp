// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.

#include "Check.h"

int main()
{
	tests::runLauncherTests();
	tests::runRetroArchTests();

	if (tests::failures == 0)
		std::puts("es-launcher: all tests passed");
	else
		std::fprintf(stderr, "es-launcher: %d check(s) failed\n", tests::failures);

	return tests::failures == 0 ? 0 : 1;
}
