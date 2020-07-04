// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <unordered_map>

#include "core/hle/kernel/object.h"

namespace Core {
class System;
} // namespace Core

namespace Core::Timing {
struct EventType;
} // namespace Core::Timing

namespace Kernel {

class Thread;

/**
 * The `TimeManager` takes care of scheduling time events on threads and executes their TimeUp
 * method when the event is triggered.
 */
class TimeManager {
public:
    explicit TimeManager(Core::System& system);

    /// Schedule a time event on `timetask` thread that will expire in 'nanoseconds'
    /// returns a non-invalid handle in `event_handle` if correctly scheduled
    void ScheduleTimeEvent(Handle& event_handle, Thread* timetask, s64 nanoseconds);

    /// Unschedule an existing time event
    void UnscheduleTimeEvent(Handle event_handle);

    void CancelTimeEvent(Thread* time_task);

private:
    Core::System& system;
    std::shared_ptr<Core::Timing::EventType> time_manager_event_type;
    std::unordered_map<Handle, bool> cancelled_events;
};

} // namespace Kernel
