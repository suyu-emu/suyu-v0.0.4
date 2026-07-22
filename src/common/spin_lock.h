// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef _MSC_VER
#include <intrin.h>
#elif defined(ARCHITECTURE_x86_64)
#include <xmmintrin.h>
#endif
#include <atomic>

namespace Common {

/// @brief A lock similar to mutex that forces a thread to spin wait instead calling the
/// supervisor. Should be used on short sequences of code.
struct SpinLock {
    SpinLock() noexcept = default;
    SpinLock(const SpinLock&) noexcept = delete;
    SpinLock& operator=(const SpinLock&) noexcept = delete;
    SpinLock(SpinLock&&) noexcept = delete;
    SpinLock& operator=(SpinLock&&) noexcept = delete;

    inline void lock() noexcept {
        while (lck.test_and_set(std::memory_order_acquire)) {
#if defined(ARCHITECTURE_x86_64)
            _mm_pause();
#elif defined(ARCHITECTURE_arm64) && defined(_MSC_VER)
            __yield();
#elif defined(ARCHITECTURE_arm64)
            asm("yield");
#endif
        }
    }

    inline void unlock() noexcept {
        lck.clear(std::memory_order_release);
    }

    [[nodiscard]] inline bool try_lock() noexcept {
        return !lck.test_and_set(std::memory_order_acquire);
    }

    std::atomic_flag lck = ATOMIC_FLAG_INIT;
};

} // namespace Common
