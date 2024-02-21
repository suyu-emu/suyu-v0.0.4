// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "audio_core/renderer/audio_renderer.h"
#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/service.h"

namespace Service::Audio {

class IAudioRenderer final : public ServiceFramework<IAudioRenderer> {
public:
    explicit IAudioRenderer(Core::System& system_, AudioCore::Renderer::Manager& manager_,
                            AudioCore::AudioRendererParameterInternal& params,
                            Kernel::KTransferMemory* transfer_memory, u64 transfer_memory_size,
                            Kernel::KProcess* process_handle_, u64 applet_resource_user_id,
                            s32 session_id);
    ~IAudioRenderer() override;

private:
    void GetSampleRate(HLERequestContext& ctx);
    void GetSampleCount(HLERequestContext& ctx);
    void GetState(HLERequestContext& ctx);
    void GetMixBufferCount(HLERequestContext& ctx);
    void RequestUpdate(HLERequestContext& ctx);
    void Start(HLERequestContext& ctx);
    void Stop(HLERequestContext& ctx);
    void QuerySystemEvent(HLERequestContext& ctx);
    void SetRenderingTimeLimit(HLERequestContext& ctx);
    void GetRenderingTimeLimit(HLERequestContext& ctx);
    void ExecuteAudioRendererRendering(HLERequestContext& ctx);
    void SetVoiceDropParameter(HLERequestContext& ctx);
    void GetVoiceDropParameter(HLERequestContext& ctx);

    KernelHelpers::ServiceContext service_context;
    Kernel::KEvent* rendered_event;
    AudioCore::Renderer::Manager& manager;
    std::unique_ptr<AudioCore::Renderer::Renderer> impl;
    Kernel::KProcess* process_handle;
    Common::ScratchBuffer<u8> output_buffer;
    Common::ScratchBuffer<u8> performance_buffer;
};

} // namespace Service::Audio
