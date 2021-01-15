// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <climits>
#include <cstddef>

#ifdef _MSC_VER
#include <intrin.h>
#endif

#include "common/common_types.h"

namespace Common {

/// Gets the size of a specified type T in bits.
template <typename T>
[[nodiscard]] constexpr std::size_t BitSize() {
    return sizeof(T) * CHAR_BIT;
}

#ifdef _MSC_VER

[[nodiscard]] inline u32 MostSignificantBit32(const u32 value) {
    unsigned long result;
    _BitScanReverse(&result, value);
    return static_cast<u32>(result);
}

[[nodiscard]] inline u32 MostSignificantBit64(const u64 value) {
    unsigned long result;
    _BitScanReverse64(&result, value);
    return static_cast<u32>(result);
}

#else

[[nodiscard]] inline u32 MostSignificantBit32(const u32 value) {
    return 31U - static_cast<u32>(__builtin_clz(value));
}

[[nodiscard]] inline u32 MostSignificantBit64(const u64 value) {
    return 63U - static_cast<u32>(__builtin_clzll(value));
}

#endif

[[nodiscard]] inline u32 Log2Floor32(const u32 value) {
    return MostSignificantBit32(value);
}

[[nodiscard]] inline u32 Log2Ceil32(const u32 value) {
    const u32 log2_f = Log2Floor32(value);
    return log2_f + ((value ^ (1U << log2_f)) != 0U);
}

[[nodiscard]] inline u32 Log2Floor64(const u64 value) {
    return MostSignificantBit64(value);
}

[[nodiscard]] inline u32 Log2Ceil64(const u64 value) {
    const u64 log2_f = static_cast<u64>(Log2Floor64(value));
    return static_cast<u32>(log2_f + ((value ^ (1ULL << log2_f)) != 0ULL));
}

} // namespace Common
