// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2015 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Use this to disable microprofile. This will get you cleaner profiles when using
// external sampling profilers like "Very Sleepy", and will improve performance somewhat.
#ifdef ANDROID
#define MICROPROFILE_ENABLED 0
#define MICROPROFILEUI_ENABLED 0
#define MicroProfileOnThreadExit() do{}while(0)
#define MICROPROFILE_TOKEN(x) 0
#define MicroProfileEnter(x) 0
#define MicroProfileLeave(x, y) ignore_all(x, y)
#endif

// Customized Citra settings.
// This file wraps the MicroProfile header so that these are consistent everywhere.
#define MICROPROFILE_WEBSERVER 0
#define MICROPROFILE_GPU_TIMERS 0 // TODO: Implement timer queries when we upgrade to OpenGL 3.3
#define MICROPROFILE_CONTEXT_SWITCH_TRACE 0
#define MICROPROFILE_PER_THREAD_BUFFER_SIZE (2048 << 13) // 16 MB

#ifdef _WIN32
// This isn't defined by the standard library in MSVC2015
typedef void* HANDLE;
#endif

#include <tuple>
template <typename... Args>
void ignore_all(Args&&... args) {
    (static_cast<void>(std::ignore = args), ...);
}

#include <microprofile.h>

#define MP_RGB(r, g, b) ((r) << 16 | (g) << 8 | (b) << 0)
