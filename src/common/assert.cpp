// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "common/common_funcs.h"
#include "common/logging/backend.h"
#include "common/settings.h"

#ifdef _MSC_VER
extern "C" {
__declspec(dllimport) void __stdcall DebugBreak(void);
}
#endif
void AssertFailSoftImpl() {
    if (Settings::values.use_debug_asserts) {
        Common::Log::Stop();
#ifndef _MSC_VER
#   if defined(ARCHITECTURE_x86_64)
        __asm__ __volatile__("int $3");
#   elif defined(ARCHITECTURE_arm64)
        __asm__ __volatile__("brk #0");
#   else
        exit(1);
#   endif
#else // POSIX ^^^ _MSC_VER vvv
        DebugBreak();
#endif
    }
}
void AssertFatalImpl() {
    Common::Log::Stop();
    std::abort();
}
