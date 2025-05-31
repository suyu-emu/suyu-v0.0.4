/* This file is part of the dynarmic project.
 * Copyright (c) 2022 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#pragma once

#include <mcl/stdint.hpp>

namespace Dynarmic::Atomic {

inline u32 Load(volatile u32* ptr) {
#ifdef _MSC_VER
    return _InterlockedOr(reinterpret_cast<volatile long*>(ptr), 0);
#else
    return __atomic_load_n(ptr, __ATOMIC_SEQ_CST);
#endif
}

inline void Or(volatile u32* ptr, u32 value) {
#ifdef _MSC_VER
    _InterlockedOr(reinterpret_cast<volatile long*>(ptr), value);
#else
    __atomic_or_fetch(ptr, value, __ATOMIC_SEQ_CST);
#endif
}

inline void And(volatile u32* ptr, u32 value) {
#ifdef _MSC_VER
    _InterlockedAnd(reinterpret_cast<volatile long*>(ptr), value);
#else
    __atomic_and_fetch(ptr, value, __ATOMIC_SEQ_CST);
#endif
}

inline void Barrier() {
#ifdef _MSC_VER
    _ReadWriteBarrier();
#else
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
#endif
}

}  // namespace Dynarmic::Atomic
