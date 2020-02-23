// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/core_timing_util.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/time_manager.h"

namespace Kernel {

TimeManager::TimeManager(Core::System& system) : system{system} {
    time_manager_event_type = Core::Timing::CreateEvent(
        "Kernel::TimeManagerCallback", [this](u64 thread_handle, [[maybe_unused]] s64 cycles_late) {
            Handle proper_handle = static_cast<Handle>(thread_handle);
            std::shared_ptr<Thread> thread =
                this->system.Kernel().RetrieveThreadFromGlobalHandleTable(proper_handle);
            thread->ResumeFromWait();
        });
}

void TimeManager::ScheduleTimeEvent(Handle& event_handle, Thread* timetask, s64 nanoseconds) {
    if (nanoseconds > 0) {
        ASSERT(timetask);
        event_handle = timetask->GetGlobalHandle();
        const s64 cycles = Core::Timing::nsToCycles(std::chrono::nanoseconds{nanoseconds});
        system.CoreTiming().ScheduleEvent(cycles, time_manager_event_type, event_handle);
    } else {
        event_handle = InvalidHandle;
    }
}

void TimeManager::UnscheduleTimeEvent(Handle event_handle) {
    if (event_handle == InvalidHandle) {
        return;
    }
    system.CoreTiming().UnscheduleEvent(time_manager_event_type, event_handle);
}

} // namespace Kernel
