// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/core_timing_util.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/k_scheduler.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/time_manager.h"

namespace Kernel {

TimeManager::TimeManager(Core::System& system_) : system{system_} {
    time_manager_event_type = Core::Timing::CreateEvent(
        "Kernel::TimeManagerCallback",
        [this](std::uintptr_t thread_handle, std::chrono::nanoseconds) {
            std::shared_ptr<Thread> thread;
            {
                std::lock_guard lock{mutex};
                const auto proper_handle = static_cast<Handle>(thread_handle);
                if (cancelled_events[proper_handle]) {
                    return;
                }
                thread = system.Kernel().RetrieveThreadFromGlobalHandleTable(proper_handle);
            }

            if (thread) {
                // Thread can be null if process has exited
                thread->Wakeup();
            }
        });
}

void TimeManager::ScheduleTimeEvent(Handle& event_handle, Thread* timetask, s64 nanoseconds) {
    std::lock_guard lock{mutex};
    event_handle = timetask->GetGlobalHandle();
    if (nanoseconds > 0) {
        ASSERT(timetask);
        ASSERT(timetask->GetState() != ThreadState::Runnable);
        system.CoreTiming().ScheduleEvent(std::chrono::nanoseconds{nanoseconds},
                                          time_manager_event_type, event_handle);
    } else {
        event_handle = InvalidHandle;
    }
    cancelled_events[event_handle] = false;
}

void TimeManager::UnscheduleTimeEvent(Handle event_handle) {
    std::lock_guard lock{mutex};
    if (event_handle == InvalidHandle) {
        return;
    }
    system.CoreTiming().UnscheduleEvent(time_manager_event_type, event_handle);
    cancelled_events[event_handle] = true;
}

void TimeManager::CancelTimeEvent(Thread* time_task) {
    std::lock_guard lock{mutex};
    const Handle event_handle = time_task->GetGlobalHandle();
    UnscheduleTimeEvent(event_handle);
}

} // namespace Kernel
