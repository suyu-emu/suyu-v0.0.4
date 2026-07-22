// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

/* This file is part of the dynarmic project.
 * Copyright (c) 2020 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#pragma once

#include <random>
#include <type_traits>

namespace detail {
inline std::mt19937 g_rand_int_generator = [] {
    std::random_device rd;
    std::mt19937 mt{rd()};
    return mt;
}();
}  // namespace detail

template<typename T>
    requires std::is_integral_v<T>
        && (!std::is_same_v<T, signed char> && !std::is_same_v<T, unsigned char>)
T RandInt(T min, T max) {
    std::uniform_int_distribution<T> rand(min, max);
    return rand(detail::g_rand_int_generator);
}
