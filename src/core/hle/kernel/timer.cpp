// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/core_timing_util.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/timer.h"

namespace Kernel {

Timer::Timer(KernelCore& kernel) : WaitObject{kernel} {}
Timer::~Timer() = default;

SharedPtr<Timer> Timer::Create(KernelCore& kernel, ResetType reset_type, std::string name) {
    SharedPtr<Timer> timer(new Timer(kernel));

    timer->reset_type = reset_type;
    timer->signaled = false;
    timer->name = std::move(name);
    timer->initial_delay = 0;
    timer->interval_delay = 0;
    timer->callback_handle = kernel.CreateTimerCallbackHandle(timer).Unwrap();

    return timer;
}

bool Timer::ShouldWait(Thread* thread) const {
    return !signaled;
}

void Timer::Acquire(Thread* thread) {
    ASSERT_MSG(!ShouldWait(thread), "object unavailable!");

    if (reset_type == ResetType::OneShot)
        signaled = false;
}

void Timer::Set(s64 initial, s64 interval) {
    // Ensure we get rid of any previous scheduled event
    Cancel();

    initial_delay = initial;
    interval_delay = interval;

    if (initial == 0) {
        // Immediately invoke the callback
        Signal(0);
    } else {
        CoreTiming::ScheduleEvent(CoreTiming::nsToCycles(initial), kernel.TimerCallbackEventType(),
                                  callback_handle);
    }
}

void Timer::Cancel() {
    CoreTiming::UnscheduleEvent(kernel.TimerCallbackEventType(), callback_handle);
}

void Timer::Clear() {
    signaled = false;
}

void Timer::WakeupAllWaitingThreads() {
    WaitObject::WakeupAllWaitingThreads();

    if (reset_type == ResetType::Pulse)
        signaled = false;
}

void Timer::Signal(int cycles_late) {
    LOG_TRACE(Kernel, "Timer {} fired", GetObjectId());

    signaled = true;

    // Resume all waiting threads
    WakeupAllWaitingThreads();

    if (interval_delay != 0) {
        // Reschedule the timer with the interval delay
        CoreTiming::ScheduleEvent(CoreTiming::nsToCycles(interval_delay) - cycles_late,
                                  kernel.TimerCallbackEventType(), callback_handle);
    }
}

} // namespace Kernel
