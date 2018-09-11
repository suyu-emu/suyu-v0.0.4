// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/am/spsm.h"

namespace Service::AM {

SPSM::SPSM() : ServiceFramework{"spsm"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "GetState"},
        {1, nullptr, "SleepSystemAndWaitAwake"},
        {2, nullptr, "Unknown1"},
        {3, nullptr, "Unknown2"},
        {4, nullptr, "GetNotificationMessageEventHandle"},
        {5, nullptr, "Unknown3"},
        {6, nullptr, "Unknown4"},
        {7, nullptr, "Unknown5"},
        {8, nullptr, "AnalyzePerformanceLogForLastSleepWakeSequence"},
        {9, nullptr, "ChangeHomeButtonLongPressingTime"},
        {10, nullptr, "Unknown6"},
        {11, nullptr, "Unknown7"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

SPSM::~SPSM() = default;

} // namespace Service::AM
