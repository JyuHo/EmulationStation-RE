// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.

#pragma once

// es-launcher is built as a shared library with hidden visibility: only what is marked
// ES_LAUNCHER_API is part of its interface.
#if defined(ES_LAUNCHER_BUILDING)
	#define ES_LAUNCHER_API __attribute__((visibility("default")))
#else
	#define ES_LAUNCHER_API
#endif
