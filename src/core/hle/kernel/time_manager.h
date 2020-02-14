// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "core/hle/kernel/object.h"

namespace Core {
class System;
} // namespace Core

namespace Core::Timing {
struct EventType;
} // namespace Core::Timing

namespace Kernel {

class Thread;

class TimeManager {
public:
    TimeManager(Core::System& system);

    void ScheduleTimeEvent(Handle& event_handle, Thread* timetask, s64 nanoseconds);

    void UnscheduleTimeEvent(Handle event_handle);

private:
    Core::System& system;
    std::shared_ptr<Core::Timing::EventType> time_manager_event_type;
};

} // namespace Kernel
