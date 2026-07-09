// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <bit>
#include <climits>
#include <cstddef>
#include <type_traits>

#include "common/common_types.h"

namespace Common {

/// Gets the size of a specified type T in bits.
template <typename T>
    requires std::is_integral_v<T>
[[nodiscard]] constexpr std::size_t BitSize() {
    return std::size_t(sizeof(T) * CHAR_BIT);
}

template<typename T>
    requires std::is_integral_v<T>
[[nodiscard]] constexpr u32 MostSignificantBit(const T value) {
    return u32(sizeof(T) * CHAR_BIT - 1 - std::countl_zero(value));
}

template<typename T>
    requires std::is_integral_v<T>
[[nodiscard]] constexpr T Log2Floor(const T value) {
    return T(MostSignificantBit<T>(value));
}

template<typename T>
    requires std::is_integral_v<T>
[[nodiscard]] constexpr T Log2Ceil(const T value) {
    const T log2_f = Log2Floor<T>(value);
    return T(log2_f + T((value ^ (T(1ULL) << log2_f)) != T(0ULL)));
}

template <typename T>
    requires std::is_integral_v<T>
[[nodiscard]] T NextPow2(T value) {
    return T(1ULL << (sizeof(T) * CHAR_BIT - std::countl_zero(value - 1U)));
}

template <size_t bit_index, typename T>
    requires std::is_integral_v<T>
[[nodiscard]] constexpr bool Bit(const T value) {
    static_assert(bit_index < BitSize<T>(), "bit_index must be smaller than size of T");
    return ((value >> bit_index) & T(1)) == T(1);
}

} // namespace Common
