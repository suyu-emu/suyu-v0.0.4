// SPDX-FileCopyrightText: 2015 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Uncomment this to disable microprofile. This will get you cleaner profiles when using
// external sampling profilers like "Very Sleepy", and will improve performance somewhat.
// #define MICROPROFILE_ENABLED 0

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

#if __has_include(<microprofile.h>)
#include <microprofile.h>
#else
// microprofile header not in include path — disable profiling entirely
#ifndef MICROPROFILE_ENABLED
#define MICROPROFILE_ENABLED 0
#endif
// Stub the symbols bootmanager.cpp calls unconditionally
#define MicroProfileOnThreadCreate(name) (void)(name)
#define MicroProfileOnThreadExit() do {} while(0)
#define MICROPROFILE_DECLARE(x)
#define MICROPROFILE_DEFINE(x, a, b, c)
#define MICROPROFILE_SCOPE(x)
#define MICROPROFILE_SCOPE_TOKEN(x)
#define MICROPROFILE_ENTERI(a, b, c)
#define MICROPROFILE_LEAVE()
#define MicroProfileFlip(ctx)
#define MicroProfileTogglePause()
#define MicroProfileShutdown()
#define MicroProfileSetForceEnable(x) do {} while(0)
#define MicroProfileForceEnable() 0
#endif

#define MP_RGB(r, g, b) ((r) << 16 | (g) << 8 | (b) << 0)
