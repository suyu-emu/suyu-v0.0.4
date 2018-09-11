// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/audio/audctl.h"

namespace Service::Audio {

AudCtl::AudCtl() : ServiceFramework{"audctl"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "GetTargetVolume"},
        {1, nullptr, "SetTargetVolume"},
        {2, nullptr, "GetTargetVolumeMin"},
        {3, nullptr, "GetTargetVolumeMax"},
        {4, nullptr, "IsTargetMute"},
        {5, nullptr, "SetTargetMute"},
        {6, nullptr, "IsTargetConnected"},
        {7, nullptr, "SetDefaultTarget"},
        {8, nullptr, "GetDefaultTarget"},
        {9, nullptr, "GetAudioOutputMode"},
        {10, nullptr, "SetAudioOutputMode"},
        {11, nullptr, "SetForceMutePolicy"},
        {12, nullptr, "GetForceMutePolicy"},
        {13, nullptr, "GetOutputModeSetting"},
        {14, nullptr, "SetOutputModeSetting"},
        {15, nullptr, "SetOutputTarget"},
        {16, nullptr, "SetInputTargetForceEnabled"},
        {17, nullptr, "SetHeadphoneOutputLevelMode"},
        {18, nullptr, "GetHeadphoneOutputLevelMode"},
        {19, nullptr, "AcquireAudioVolumeUpdateEventForPlayReport"},
        {20, nullptr, "AcquireAudioOutputDeviceUpdateEventForPlayReport"},
        {21, nullptr, "GetAudioOutputTargetForPlayReport"},
        {22, nullptr, "NotifyHeadphoneVolumeWarningDisplayedEvent"},
        {23, nullptr, "SetSystemOutputMasterVolume"},
        {24, nullptr, "GetSystemOutputMasterVolume"},
        {25, nullptr, "GetAudioVolumeDataForPlayReport"},
        {26, nullptr, "UpdateHeadphoneSettings"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

AudCtl::~AudCtl() = default;

} // namespace Service::Audio
