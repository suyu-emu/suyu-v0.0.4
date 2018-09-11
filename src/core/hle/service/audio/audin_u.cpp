// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/service/audio/audin_u.h"

namespace Service::Audio {

class IAudioIn final : public ServiceFramework<IAudioIn> {
public:
    IAudioIn() : ServiceFramework("IAudioIn") {
        static const FunctionInfo functions[] = {
            {0, nullptr, "GetAudioInState"},
            {1, nullptr, "StartAudioIn"},
            {2, nullptr, "StopAudioIn"},
            {3, nullptr, "AppendAudioInBuffer"},
            {4, nullptr, "RegisterBufferEvent"},
            {5, nullptr, "GetReleasedAudioInBuffer"},
            {6, nullptr, "ContainsAudioInBuffer"},
            {7, nullptr, "AppendAudioInBufferWithUserEvent"},
            {8, nullptr, "AppendAudioInBufferAuto"},
            {9, nullptr, "GetReleasedAudioInBufferAuto"},
            {10, nullptr, "AppendAudioInBufferWithUserEventAuto"},
            {11, nullptr, "GetAudioInBufferCount"},
            {12, nullptr, "SetAudioInDeviceGain"},
            {13, nullptr, "GetAudioInDeviceGain"},
        };
        RegisterHandlers(functions);
    }
    ~IAudioIn() = default;
};

AudInU::AudInU() : ServiceFramework("audin:u") {
    static const FunctionInfo functions[] = {
        {0, nullptr, "ListAudioIns"},    {1, nullptr, "OpenAudioIn"},      {2, nullptr, "Unknown"},
        {3, nullptr, "OpenAudioInAuto"}, {4, nullptr, "ListAudioInsAuto"},
    };
    RegisterHandlers(functions);
}

AudInU::~AudInU() = default;

} // namespace Service::Audio
