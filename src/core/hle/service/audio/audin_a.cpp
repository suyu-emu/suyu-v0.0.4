// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/audio/audin_a.h"

namespace Service::Audio {

AudInA::AudInA() : ServiceFramework{"audin:a"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "RequestSuspendAudioIns"},
        {1, nullptr, "RequestResumeAudioIns"},
        {2, nullptr, "GetAudioInsProcessMasterVolume"},
        {3, nullptr, "SetAudioInsProcessMasterVolume"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

} // namespace Service::Audio
