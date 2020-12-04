// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// This file references various implementation details from Atmosphere, an open-source firmware for
// the Nintendo Switch. Copyright 2018-2020 Atmosphere-NX.

#pragma once

#include "common/common_types.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/time_manager.h"

namespace Kernel {

class KScopedSchedulerLockAndSleep {
private:
    KernelCore& kernel;
    s64 timeout_tick{};
    Thread* thread{};
    Handle* event_handle{};

public:
    explicit KScopedSchedulerLockAndSleep(KernelCore& kernel, Thread* t, s64 timeout)
        : kernel(kernel), timeout_tick(timeout), thread(t) {
        /* Lock the scheduler. */
        kernel.GlobalSchedulerContext().scheduler_lock.Lock();
    }

    explicit KScopedSchedulerLockAndSleep(KernelCore& kernel, Handle& event_handle, Thread* t,
                                          s64 timeout)
        : kernel(kernel), event_handle(&event_handle), timeout_tick(timeout), thread(t) {
        /* Lock the scheduler. */
        kernel.GlobalSchedulerContext().scheduler_lock.Lock();
    }

    ~KScopedSchedulerLockAndSleep() {
        /* Register the sleep. */
        if (this->timeout_tick > 0) {
            auto& time_manager = kernel.TimeManager();
            Handle handle{};
            time_manager.ScheduleTimeEvent(event_handle ? *event_handle : handle, this->thread,
                                           this->timeout_tick);
        }

        /* Unlock the scheduler. */
        kernel.GlobalSchedulerContext().scheduler_lock.Unlock();
    }

    void CancelSleep() {
        this->timeout_tick = 0;
    }
};

} // namespace Kernel
