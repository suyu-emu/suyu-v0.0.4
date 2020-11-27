// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/glue/bgtc.h"

namespace Service::Glue {

BGTC_T::BGTC_T(Core::System& system_) : ServiceFramework{system_, "bgtc:t"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {1, nullptr, "NotifyTaskStarting"},
        {2, nullptr, "NotifyTaskFinished"},
        {3, nullptr, "GetTriggerEvent"},
        {4, nullptr, "IsInHalfAwake"},
        {5, nullptr, "NotifyClientName"},
        {6, nullptr, "IsInFullAwake"},
        {11, nullptr, "ScheduleTask"},
        {12, nullptr, "GetScheduledTaskInterval"},
        {13, nullptr, "UnscheduleTask"},
        {14, nullptr, "GetScheduleEvent"},
        {15, nullptr, "SchedulePeriodicTask"},
        {101, nullptr, "GetOperationMode"},
        {102, nullptr, "WillDisconnectNetworkWhenEnteringSleep"},
        {103, nullptr, "WillStayHalfAwakeInsteadSleep"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

BGTC_T::~BGTC_T() = default;

BGTC_SC::BGTC_SC(Core::System& system_) : ServiceFramework{system_, "bgtc:sc"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {1, nullptr, "GetState"},
        {2, nullptr, "GetStateChangedEvent"},
        {3, nullptr, "NotifyEnteringHalfAwake"},
        {4, nullptr, "NotifyLeavingHalfAwake"},
        {5, nullptr, "SetIsUsingSleepUnsupportedDevices"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

BGTC_SC::~BGTC_SC() = default;

} // namespace Service::Glue
