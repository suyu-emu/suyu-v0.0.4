// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2013 Dolphin Emulator Project
// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>

#include "common/error.h"
#include "common/logging/log.h"
#include "common/thread.h"
#ifdef __APPLE__
#include <mach/mach.h>
#elif defined(__HAIKU__)
#include <kernel/OS.h>
#elif defined(_WIN32)
#include <windows.h>
#include "common/string_util.h"
#else
#if defined(__Bitrig__) || defined(__DragonFly__) || defined(__FreeBSD__) || defined(__OpenBSD__)
#include <pthread_np.h>
#endif
#include <pthread.h>
#include <sched.h>
#endif
#ifndef _WIN32
#include <unistd.h>
#endif

#ifdef __FreeBSD__
#define cpu_set_t cpuset_t
#endif

namespace Common {

void SetCurrentThreadPriority(ThreadPriority new_priority) {
#ifdef _WIN32
    int windows_priority = [&]() {
        switch (new_priority) {
        case ThreadPriority::Low: return THREAD_PRIORITY_BELOW_NORMAL;
        case ThreadPriority::Normal: return THREAD_PRIORITY_NORMAL;
        case ThreadPriority::High: return THREAD_PRIORITY_ABOVE_NORMAL;
        case ThreadPriority::VeryHigh: return THREAD_PRIORITY_HIGHEST;
        case ThreadPriority::Critical: return THREAD_PRIORITY_TIME_CRITICAL;
        default: return THREAD_PRIORITY_NORMAL;
        }
    }();
    SetThreadPriority(GetCurrentThread(), windows_priority);
#elif defined(__HAIKU__)
    // TODO: We have priorities for 3D rendering applications - may help lavapipe?
    int priority = [&]() {
        switch (new_priority) {
        case ThreadPriority::Low: return B_LOW_PRIORITY;
        case ThreadPriority::Normal: return B_NORMAL_PRIORITY;
        case ThreadPriority::High: return B_DISPLAY_PRIORITY;
        case ThreadPriority::VeryHigh: return B_URGENT_DISPLAY_PRIORITY;
        case ThreadPriority::Critical: return B_URGENT_PRIORITY;
        default: return B_NORMAL_PRIORITY;
        }
    }();
    set_thread_priority(find_thread(NULL), priority);
#else
    pthread_t this_thread = pthread_self();
    const auto scheduling_type = SCHED_OTHER;
    s32 max_prio = sched_get_priority_max(scheduling_type);
    s32 min_prio = sched_get_priority_min(scheduling_type);
    u32 level = (std::max)(u32(new_priority) + 1, 4U);

    struct sched_param params;
    if (max_prio > min_prio) {
        params.sched_priority = min_prio + ((max_prio - min_prio) * level) / 4;
    } else {
        params.sched_priority = min_prio - ((min_prio - max_prio) * level) / 4;
    }

    pthread_setschedparam(this_thread, scheduling_type, &params);
#endif
}

#ifdef _MSC_VER

// Sets the debugger-visible name of the current thread.
void SetCurrentThreadName(const char* name) {
    if (auto pf = (decltype(&SetThreadDescription))(void*)GetProcAddress(GetModuleHandle(TEXT("KernelBase.dll")), "SetThreadDescription"); pf)
        pf(GetCurrentThread(), UTF8ToUTF16W(name).data()); // Windows 10+
    else
        ; // No-op
}

#else // !MSVC_VER, so must be POSIX threads

// MinGW with the POSIX threading model does not support pthread_setname_np
void SetCurrentThreadName(const char* name) {
    // See for reference
    // https://gitlab.freedesktop.org/mesa/mesa/-/blame/main/src/util/u_thread.c?ref_type=heads#L75
#ifdef __APPLE__
    pthread_setname_np(name);
#elif defined(__HAIKU__)
    rename_thread(find_thread(NULL), name);
#elif defined(__Bitrig__) || defined(__DragonFly__) || defined(__FreeBSD__) || defined(__OpenBSD__)
    pthread_set_name_np(pthread_self(), name);
#elif defined(__NetBSD__)
    pthread_setname_np(pthread_self(), "%s", (void*)name);
#elif defined(__linux__) || defined(__CYGWIN__) || defined(__sun__) || defined(__glibc__) || defined(__managarm__)
    int ret = pthread_setname_np(pthread_self(), name);
    if (ret == ERANGE) {
        // Linux limits thread names to 15 characters and will outright reject any
        // attempt to set a longer name with ERANGE.
        char buf[16];
        size_t const len = std::min<size_t>(std::strlen(name), sizeof(buf) - 1);
        std::memcpy(buf, name, len);
        buf[len] = '\0';
        pthread_setname_np(pthread_self(), buf);
    }
#elif defined(_WIN32)
    // mingw stub
    (void)name;
#else
    pthread_setname_np(pthread_self(), name);
#endif
}

#endif

} // namespace Common
