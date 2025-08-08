// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2013 Dolphin Emulator Project
// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <fmt/format.h>

[[noreturn]] void assert_terminate_impl(const char* expr_str, fmt::string_view msg, fmt::format_args args);
template<typename... Ts>
[[noreturn]] void assert_terminate(const char* expr_str, fmt::string_view msg, Ts... args) {
    assert_terminate_impl(expr_str, msg, fmt::make_format_args(args...));
}

// Temporary until MCL is fully removed
#ifndef ASSERT_MSG
#define ASSERT_MSG(_a_, ...)                                                                       \
    ([&]() {                                                                        \
        if (!(_a_)) [[unlikely]] {                                                                 \
            assert_terminate(#_a_, __VA_ARGS__);                                                   \
        }                                                                                          \
    }())
#endif

#ifndef ASSERT
#define ASSERT(_a_) ASSERT_MSG(_a_, "")
#endif
#ifndef UNREACHABLE
#define UNREACHABLE() ASSERT_MSG(false, "unreachable")
#endif
#ifdef _DEBUG
#ifndef DEBUG_ASSERT
#define DEBUG_ASSERT(_a_) ASSERT(_a_)
#endif
#ifndef DEBUG_ASSERT_MSG
#define DEBUG_ASSERT_MSG(_a_, ...) ASSERT_MSG(_a_, __VA_ARGS__)
#endif
#else // not debug
#ifndef DEBUG_ASSERT
#define DEBUG_ASSERT(_a_)
#endif
#ifndef DEBUG_ASSERT_MSG
#define DEBUG_ASSERT_MSG(_a_, _desc_, ...)
#endif
#endif
