// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// This file references various implementation details from Atmosphere, an open-source firmware for
// the Nintendo Switch. Copyright 2018-2020 Atmosphere-NX.

#pragma once

#include "common/assert.h"
#include "common/spin_lock.h"
#include "core/hardware_properties.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/kernel.h"

namespace Kernel {

class KernelCore;

template <typename SchedulerType>
class KAbstractSchedulerLock {
public:
    explicit KAbstractSchedulerLock(KernelCore& kernel_) : kernel{kernel_} {}

    bool IsLockedByCurrentThread() const {
        return owner_thread == GetCurrentThreadPointer(kernel);
    }

    void Lock() {
        if (IsLockedByCurrentThread()) {
            // If we already own the lock, we can just increment the count.
            ASSERT(lock_count > 0);
            lock_count++;
        } else {
            // Otherwise, we want to disable scheduling and acquire the spinlock.
            SchedulerType::DisableScheduling(kernel);
            spin_lock.lock();

            // For debug, ensure that our state is valid.
            ASSERT(lock_count == 0);
            ASSERT(owner_thread == nullptr);

            // Increment count, take ownership.
            lock_count = 1;
            owner_thread = GetCurrentThreadPointer(kernel);
        }
    }

    void Unlock() {
        ASSERT(IsLockedByCurrentThread());
        ASSERT(lock_count > 0);

        // Release an instance of the lock.
        if ((--lock_count) == 0) {
            // We're no longer going to hold the lock. Take note of what cores need scheduling.
            const u64 cores_needing_scheduling =
                SchedulerType::UpdateHighestPriorityThreads(kernel);

            // Note that we no longer hold the lock, and unlock the spinlock.
            owner_thread = nullptr;
            spin_lock.unlock();

            // Enable scheduling, and perform a rescheduling operation.
            SchedulerType::EnableScheduling(kernel, cores_needing_scheduling);
        }
    }

private:
    KernelCore& kernel;
    Common::SpinLock spin_lock{};
    s32 lock_count{};
    KThread* owner_thread{};
};

} // namespace Kernel
