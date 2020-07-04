// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>

#include "common/atomic_ops.h"

#if _MSC_VER
#include <intrin.h>
#endif

namespace Common {

#if _MSC_VER

bool AtomicCompareAndSwap(u8 volatile* pointer, u8 value, u8 expected) {
    u8 result = _InterlockedCompareExchange8((char*)pointer, value, expected);
    return result == expected;
}

bool AtomicCompareAndSwap(u16 volatile* pointer, u16 value, u16 expected) {
    u16 result = _InterlockedCompareExchange16((short*)pointer, value, expected);
    return result == expected;
}

bool AtomicCompareAndSwap(u32 volatile* pointer, u32 value, u32 expected) {
    u32 result = _InterlockedCompareExchange((long*)pointer, value, expected);
    return result == expected;
}

bool AtomicCompareAndSwap(u64 volatile* pointer, u64 value, u64 expected) {
    u64 result = _InterlockedCompareExchange64((__int64*)pointer, value, expected);
    return result == expected;
}

bool AtomicCompareAndSwap(u64 volatile* pointer, u128 value, u128 expected) {
    return _InterlockedCompareExchange128((__int64*)pointer, value[1], value[0],
                                          (__int64*)expected.data()) != 0;
}

#else

bool AtomicCompareAndSwap(u8 volatile* pointer, u8 value, u8 expected) {
    return __sync_bool_compare_and_swap(pointer, expected, value);
}

bool AtomicCompareAndSwap(u16 volatile* pointer, u16 value, u16 expected) {
    return __sync_bool_compare_and_swap(pointer, expected, value);
}

bool AtomicCompareAndSwap(u32 volatile* pointer, u32 value, u32 expected) {
    return __sync_bool_compare_and_swap(pointer, expected, value);
}

bool AtomicCompareAndSwap(u64 volatile* pointer, u64 value, u64 expected) {
    return __sync_bool_compare_and_swap(pointer, expected, value);
}

bool AtomicCompareAndSwap(u64 volatile* pointer, u128 value, u128 expected) {
    unsigned __int128 value_a;
    unsigned __int128 expected_a;
    std::memcpy(&value_a, value.data(), sizeof(u128));
    std::memcpy(&expected_a, expected.data(), sizeof(u128));
    return __sync_bool_compare_and_swap((unsigned __int128*)pointer, expected_a, value_a);
}

#endif

} // namespace Common
