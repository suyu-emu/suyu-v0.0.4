// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/audio/audout_a.h"

namespace Service::Audio {

AudOutA::AudOutA() : ServiceFramework{"audout:a"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "RequestSuspendAudioOuts"},
        {1, nullptr, "RequestResumeAudioOuts"},
        {2, nullptr, "GetAudioOutsProcessMasterVolume"},
        {3, nullptr, "SetAudioOutsProcessMasterVolume"},
        {4, nullptr, "GetAudioOutsProcessRecordVolume"},
        {5, nullptr, "SetAudioOutsProcessRecordVolume"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

AudOutA::~AudOutA() = default;

} // namespace Service::Audio
