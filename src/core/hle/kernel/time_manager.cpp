// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/core_timing_util.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/scheduler.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/time_manager.h"

namespace Kernel {

TimeManager::TimeManager(Core::System& system_) : system{system_} {
    time_manager_event_type = Core::Timing::CreateEvent(
        "Kernel::TimeManagerCallback", [this](u64 thread_handle, [[maybe_unused]] s64 cycles_late) {
            SchedulerLock lock(system.Kernel());
            Handle proper_handle = static_cast<Handle>(thread_handle);
            if (cancelled_events[proper_handle]) {
                return;
            }
            std::shared_ptr<Thread> thread =
                this->system.Kernel().RetrieveThreadFromGlobalHandleTable(proper_handle);
            thread->OnWakeUp();
        });
}

void TimeManager::ScheduleTimeEvent(Handle& event_handle, Thread* timetask, s64 nanoseconds) {
    event_handle = timetask->GetGlobalHandle();
    if (nanoseconds > 0) {
        ASSERT(timetask);
        ASSERT(timetask->GetStatus() != ThreadStatus::Ready);
        ASSERT(timetask->GetStatus() != ThreadStatus::WaitMutex);
        system.CoreTiming().ScheduleEvent(nanoseconds, time_manager_event_type, event_handle);
    } else {
        event_handle = InvalidHandle;
    }
    cancelled_events[event_handle] = false;
}

void TimeManager::UnscheduleTimeEvent(Handle event_handle) {
    if (event_handle == InvalidHandle) {
        return;
    }
    system.CoreTiming().UnscheduleEvent(time_manager_event_type, event_handle);
    cancelled_events[event_handle] = true;
}

void TimeManager::CancelTimeEvent(Thread* time_task) {
    Handle event_handle = time_task->GetGlobalHandle();
    UnscheduleTimeEvent(event_handle);
}

} // namespace Kernel
