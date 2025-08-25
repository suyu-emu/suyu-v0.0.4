// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <fmt/format.h>
#include <cstdio>
#include <exception>

[[noreturn]] void assert_terminate_impl(const char* expr_str, fmt::string_view msg, fmt::format_args args) {
    fmt::print(stderr, "assertion failed: {}\n", expr_str);
    fmt::vprint(stderr, msg, args);
    std::fflush(stderr);
    std::terminate();
}
