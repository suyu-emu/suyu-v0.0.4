// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdio>
#include <exception>

[[noreturn]] void assert_terminate_impl(const char* s) {
    std::puts(s);
    std::fflush(stderr);
    std::terminate();
}
