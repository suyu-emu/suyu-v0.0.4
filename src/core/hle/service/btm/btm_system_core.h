// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/hle/service/cmif_types.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::BTM {

class IBtmSystemCore final : public ServiceFramework<IBtmSystemCore> {
public:
    explicit IBtmSystemCore(Core::System& system_);
    ~IBtmSystemCore() override;

private:
    Result StartGamepadPairing();
    Result CancelGamepadPairing();
    Result IsRadioEnabled(Out<bool> out_is_enabled);

    Result GetConnectedAudioDevices(
        Out<s32> out_count,
        OutArray<std::array<u8, 0xFF>, BufferAttr_HipcPointer> out_audio_devices);

    Result GetPairedAudioDevices(
        Out<s32> out_count,
        OutArray<std::array<u8, 0xFF>, BufferAttr_HipcPointer> out_audio_devices);

    Result RequestAudioDeviceConnectionRejection(ClientAppletResourceUserId aruid);
    Result CancelAudioDeviceConnectionRejection(ClientAppletResourceUserId aruid);
};

} // namespace Service::BTM
