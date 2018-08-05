// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/audio/audren_a.h"

namespace Service::Audio {

AudRenA::AudRenA() : ServiceFramework{"audren:a"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "RequestSuspendAudioRenderers"},
        {1, nullptr, "RequestResumeAudioRenderers"},
        {2, nullptr, "GetAudioRenderersProcessMasterVolume"},
        {3, nullptr, "SetAudioRenderersProcessMasterVolume"},
        {4, nullptr, "RegisterAppletResourceUserId"},
        {5, nullptr, "UnregisterAppletResourceUserId"},
        {6, nullptr, "GetAudioRenderersProcessRecordVolume"},
        {7, nullptr, "SetAudioRenderersProcessRecordVolume"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

} // namespace Service::Audio
