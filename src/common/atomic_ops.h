// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/common_types.h"

#if _MSC_VER
#include <intrin.h>
#else
#include <cstring>
#endif

namespace Common {

#if _MSC_VER

template <typename T>
[[nodiscard]] inline bool AtomicCompareAndSwap(T* pointer, T value, T expected);
template <typename T>
[[nodiscard]] inline bool AtomicCompareAndSwap(T* pointer, T value, T expected, T& actual);

template <>
[[nodiscard]] inline bool AtomicCompareAndSwap<u8>(u8* pointer, u8 value, u8 expected) {
    const u8 result =
        _InterlockedCompareExchange8(reinterpret_cast<volatile char*>(pointer), value, expected);
    return result == expected;
}

template <>
[[nodiscard]] inline bool AtomicCompareAndSwap<u16>(u16* pointer, u16 value, u16 expected) {
    const u16 result =
        _InterlockedCompareExchange16(reinterpret_cast<volatile short*>(pointer), value, expected);
    return result == expected;
}

template <>
[[nodiscard]] inline bool AtomicCompareAndSwap<u32>(u32* pointer, u32 value, u32 expected) {
    const u32 result =
        _InterlockedCompareExchange(reinterpret_cast<volatile long*>(pointer), value, expected);
    return result == expected;
}

template <>
[[nodiscard]] inline bool AtomicCompareAndSwap<u64>(u64* pointer, u64 value, u64 expected) {
    const u64 result = _InterlockedCompareExchange64(reinterpret_cast<volatile __int64*>(pointer),
                                                     value, expected);
    return result == expected;
}

[[nodiscard]] inline bool AtomicCompareAndSwap(volatile u64* pointer, u128 value, u128 expected) {
    return _InterlockedCompareExchange128(reinterpret_cast<volatile __int64*>(pointer), value[1],
                                          value[0],
                                          reinterpret_cast<__int64*>(expected.data())) != 0;
}

template <>
[[nodiscard]] inline bool AtomicCompareAndSwap<u8>(u8* pointer, u8 value, u8 expected, u8& actual) {
    actual =
        _InterlockedCompareExchange8(reinterpret_cast<volatile char*>(pointer), value, expected);
    return actual == expected;
}

template <>
[[nodiscard]] inline bool AtomicCompareAndSwap<u16>(u16* pointer, u16 value, u16 expected,
                                                    u16& actual) {
    actual =
        _InterlockedCompareExchange16(reinterpret_cast<volatile short*>(pointer), value, expected);
    return actual == expected;
}

template <>
[[nodiscard]] inline bool AtomicCompareAndSwap<u32>(u32* pointer, u32 value, u32 expected,
                                                    u32& actual) {
    actual =
        _InterlockedCompareExchange(reinterpret_cast<volatile long*>(pointer), value, expected);
    return actual == expected;
}

template <>
[[nodiscard]] inline bool AtomicCompareAndSwap<u64>(u64* pointer, u64 value, u64 expected,
                                                    u64& actual) {
    actual = _InterlockedCompareExchange64(reinterpret_cast<volatile __int64*>(pointer), value,
                                           expected);
    return actual == expected;
}

[[nodiscard]] inline bool AtomicCompareAndSwap(volatile u64* pointer, u128 value, u128 expected,
                                               u128& actual) {
    const bool result =
        _InterlockedCompareExchange128(reinterpret_cast<volatile __int64*>(pointer), value[1],
                                       value[0], reinterpret_cast<__int64*>(expected.data())) != 0;
    actual = expected;
    return result;
}

[[nodiscard]] inline u128 AtomicLoad128(volatile u64* pointer) {
    u128 result{};
    _InterlockedCompareExchange128(reinterpret_cast<volatile __int64*>(pointer), result[1],
                                   result[0], reinterpret_cast<__int64*>(result.data()));
    return result;
}

#else

// Some architectures lack u128, there is no definitive way to check them all without even more macro magic
// so let's just... do this; add your favourite arches once they get u128 support :)
#if (defined(__clang__) || defined(__GNUC__)) && (defined(ARCHITECTURE_x86_64) || defined(ARCHITECTURE_arm64))
using RealU128 = unsigned __int128;
#   define SYNC_VAL_COMPARE_AND_SWAP(p, e, v) __sync_val_compare_and_swap(p, e, v)
#   define SYNC_BOOL_COMPARE_AND_SWAP(p, e, v) __sync_bool_compare_and_swap(p, e, v)
#   define U128_ZERO_INIT 0
#else
using RealU128 = u128;
#   define SYNC_VAL_COMPARE_AND_SWAP(p, e, v) ((*p == e) ? *p = v : *p)
#   define SYNC_BOOL_COMPARE_AND_SWAP(p, e, v) ((*p == e) ? (void)(*p = v) : (void)0), true
#   define U128_ZERO_INIT {}
#endif

template <typename T>
[[nodiscard]] inline bool AtomicCompareAndSwap(T* pointer, T value, T expected) {
    return SYNC_BOOL_COMPARE_AND_SWAP(pointer, expected, value);
}

[[nodiscard]] inline bool AtomicCompareAndSwap(u64* pointer, u128 value, u128 expected) {
    RealU128 value_a;
    RealU128 expected_a;
    std::memcpy(&value_a, value.data(), sizeof(u128));
    std::memcpy(&expected_a, expected.data(), sizeof(u128));
    return SYNC_BOOL_COMPARE_AND_SWAP((RealU128*)pointer, expected_a, value_a);
}

template <typename T>
[[nodiscard]] inline bool AtomicCompareAndSwap(T* pointer, T value, T expected, T& actual) {
    actual = SYNC_VAL_COMPARE_AND_SWAP(pointer, expected, value);
    return actual == expected;
}

[[nodiscard]] inline bool AtomicCompareAndSwap(u64* pointer, u128 value, u128 expected, u128& actual) {
    RealU128 value_a;
    RealU128 expected_a;
    RealU128 actual_a;
    std::memcpy(&value_a, value.data(), sizeof(u128));
    std::memcpy(&expected_a, expected.data(), sizeof(u128));
    actual_a = SYNC_VAL_COMPARE_AND_SWAP((RealU128*)pointer, expected_a, value_a);
    std::memcpy(actual.data(), &actual_a, sizeof(u128));
    return actual_a == expected_a;
}

[[nodiscard]] inline u128 AtomicLoad128(u64* pointer) {
    RealU128 zeros_a = U128_ZERO_INIT;
    RealU128 result_a = SYNC_VAL_COMPARE_AND_SWAP((RealU128*)pointer, zeros_a, zeros_a);
    u128 result;
    std::memcpy(result.data(), &result_a, sizeof(u128));
    return result;
}
#undef U128_ZERO_INIT
#undef SYNC_VAL_COMPARE_AND_SWAP
#undef SYNC_BOOL_COMPARE_AND_SWAP

#endif

} // namespace Common
