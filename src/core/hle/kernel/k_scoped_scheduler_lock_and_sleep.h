// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// This file references various implementation details from Atmosphere, an open-source firmware for
// the Nintendo Switch. Copyright 2018-2020 Atmosphere-NX.

#pragma once

#include "common/common_types.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/time_manager.h"

namespace Kernel {

class KScopedSchedulerLockAndSleep {
public:
    explicit KScopedSchedulerLockAndSleep(KernelCore& kernel, Handle& event_handle, Thread* t,
                                          s64 timeout)
        : kernel(kernel), event_handle(event_handle), thread(t), timeout_tick(timeout) {
        event_handle = InvalidHandle;

        // Lock the scheduler.
        kernel.GlobalSchedulerContext().scheduler_lock.Lock();
    }

    ~KScopedSchedulerLockAndSleep() {
        // Register the sleep.
        if (this->timeout_tick > 0) {
            kernel.TimeManager().ScheduleTimeEvent(event_handle, this->thread, this->timeout_tick);
        }

        // Unlock the scheduler.
        kernel.GlobalSchedulerContext().scheduler_lock.Unlock();
    }

    void CancelSleep() {
        this->timeout_tick = 0;
    }

private:
    KernelCore& kernel;
    Handle& event_handle;
    Thread* thread{};
    s64 timeout_tick{};
};

} // namespace Kernel
