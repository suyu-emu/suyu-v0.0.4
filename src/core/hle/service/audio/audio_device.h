// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "audio_core/renderer/audio_device.h"
#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/service.h"

namespace Service::Audio {

class IAudioDevice final : public ServiceFramework<IAudioDevice> {

public:
    explicit IAudioDevice(Core::System& system_, u64 applet_resource_user_id, u32 revision,
                          u32 device_num);
    ~IAudioDevice() override;

private:
    void ListAudioDeviceName(HLERequestContext& ctx);
    void SetAudioDeviceOutputVolume(HLERequestContext& ctx);
    void GetAudioDeviceOutputVolume(HLERequestContext& ctx);
    void GetActiveAudioDeviceName(HLERequestContext& ctx);
    void QueryAudioDeviceSystemEvent(HLERequestContext& ctx);
    void GetActiveChannelCount(HLERequestContext& ctx);
    void QueryAudioDeviceInputEvent(HLERequestContext& ctx);
    void QueryAudioDeviceOutputEvent(HLERequestContext& ctx);
    void ListAudioOutputDeviceName(HLERequestContext& ctx);

    KernelHelpers::ServiceContext service_context;
    std::unique_ptr<AudioCore::Renderer::AudioDevice> impl;
    Kernel::KEvent* event;
};

} // namespace Service::Audio
