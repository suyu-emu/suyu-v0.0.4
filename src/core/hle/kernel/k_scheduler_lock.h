// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// This file references various implementation details from Atmosphere, an open-source firmware for
// the Nintendo Switch. Copyright 2018-2020 Atmosphere-NX.

#pragma once

#include "common/assert.h"
#include "common/spin_lock.h"
#include "core/hardware_properties.h"

namespace Kernel {

class KernelCore;

template <typename SchedulerType>
class KAbstractSchedulerLock {
public:
    explicit KAbstractSchedulerLock(KernelCore& kernel) : kernel{kernel} {}

    bool IsLockedByCurrentThread() const {
        return this->owner_thread == kernel.GetCurrentEmuThreadID();
    }

    void Lock() {
        if (this->IsLockedByCurrentThread()) {
            // If we already own the lock, we can just increment the count.
            ASSERT(this->lock_count > 0);
            this->lock_count++;
        } else {
            // Otherwise, we want to disable scheduling and acquire the spinlock.
            SchedulerType::DisableScheduling(kernel);
            this->spin_lock.lock();

            // For debug, ensure that our state is valid.
            ASSERT(this->lock_count == 0);
            ASSERT(this->owner_thread == Core::EmuThreadHandle::InvalidHandle());

            // Increment count, take ownership.
            this->lock_count = 1;
            this->owner_thread = kernel.GetCurrentEmuThreadID();
        }
    }

    void Unlock() {
        ASSERT(this->IsLockedByCurrentThread());
        ASSERT(this->lock_count > 0);

        // Release an instance of the lock.
        if ((--this->lock_count) == 0) {
            // We're no longer going to hold the lock. Take note of what cores need scheduling.
            const u64 cores_needing_scheduling =
                SchedulerType::UpdateHighestPriorityThreads(kernel);
            Core::EmuThreadHandle leaving_thread = owner_thread;

            // Note that we no longer hold the lock, and unlock the spinlock.
            this->owner_thread = Core::EmuThreadHandle::InvalidHandle();
            this->spin_lock.unlock();

            // Enable scheduling, and perform a rescheduling operation.
            SchedulerType::EnableScheduling(kernel, cores_needing_scheduling, leaving_thread);
        }
    }

private:
    KernelCore& kernel;
    Common::SpinLock spin_lock{};
    s32 lock_count{};
    Core::EmuThreadHandle owner_thread{Core::EmuThreadHandle::InvalidHandle()};
};

} // namespace Kernel
