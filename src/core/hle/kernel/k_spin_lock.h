// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>

#include "core/hle/kernel/k_scoped_lock.h"

namespace Kernel {

class KSpinLock {
public:
    KSpinLock() = default;

    KSpinLock(const KSpinLock&) = delete;
    KSpinLock& operator=(const KSpinLock&) = delete;

    KSpinLock(KSpinLock&&) = delete;
    KSpinLock& operator=(KSpinLock&&) = delete;

    void Lock();
    void Unlock();
    [[nodiscard]] bool TryLock();

private:
    std::atomic_flag lck = ATOMIC_FLAG_INIT;
};

using KScopedSpinLock = KScopedLock<KSpinLock>;

} // namespace Kernel
