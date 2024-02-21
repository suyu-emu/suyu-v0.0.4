// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/audio/audio_renderer.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::Audio {
using namespace AudioCore::Renderer;

IAudioRenderer::IAudioRenderer(Core::System& system_, Manager& manager_,
                               AudioCore::AudioRendererParameterInternal& params,
                               Kernel::KTransferMemory* transfer_memory, u64 transfer_memory_size,
                               Kernel::KProcess* process_handle_, u64 applet_resource_user_id,
                               s32 session_id)
    : ServiceFramework{system_, "IAudioRenderer"}, service_context{system_, "IAudioRenderer"},
      rendered_event{service_context.CreateEvent("IAudioRendererEvent")}, manager{manager_},
      impl{std::make_unique<Renderer>(system_, manager, rendered_event)},
      process_handle{process_handle_} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &IAudioRenderer::GetSampleRate, "GetSampleRate"},
        {1, &IAudioRenderer::GetSampleCount, "GetSampleCount"},
        {2, &IAudioRenderer::GetMixBufferCount, "GetMixBufferCount"},
        {3, &IAudioRenderer::GetState, "GetState"},
        {4, &IAudioRenderer::RequestUpdate, "RequestUpdate"},
        {5, &IAudioRenderer::Start, "Start"},
        {6, &IAudioRenderer::Stop, "Stop"},
        {7, &IAudioRenderer::QuerySystemEvent, "QuerySystemEvent"},
        {8, &IAudioRenderer::SetRenderingTimeLimit, "SetRenderingTimeLimit"},
        {9, &IAudioRenderer::GetRenderingTimeLimit, "GetRenderingTimeLimit"},
        {10, &IAudioRenderer::RequestUpdate, "RequestUpdateAuto"},
        {11, nullptr, "ExecuteAudioRendererRendering"},
        {12, &IAudioRenderer::SetVoiceDropParameter, "SetVoiceDropParameter"},
        {13, &IAudioRenderer::GetVoiceDropParameter, "GetVoiceDropParameter"},
    };
    // clang-format on
    RegisterHandlers(functions);

    process_handle->Open();
    impl->Initialize(params, transfer_memory, transfer_memory_size, process_handle,
                     applet_resource_user_id, session_id);
}

IAudioRenderer::~IAudioRenderer() {
    impl->Finalize();
    service_context.CloseEvent(rendered_event);
    process_handle->Close();
}

void IAudioRenderer::GetSampleRate(HLERequestContext& ctx) {
    const auto sample_rate{impl->GetSystem().GetSampleRate()};

    LOG_DEBUG(Service_Audio, "called. Sample rate {}", sample_rate);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(sample_rate);
}

void IAudioRenderer::GetSampleCount(HLERequestContext& ctx) {
    const auto sample_count{impl->GetSystem().GetSampleCount()};

    LOG_DEBUG(Service_Audio, "called. Sample count {}", sample_count);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(sample_count);
}

void IAudioRenderer::GetState(HLERequestContext& ctx) {
    const u32 state{!impl->GetSystem().IsActive()};

    LOG_DEBUG(Service_Audio, "called, state {}", state);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(state);
}

void IAudioRenderer::GetMixBufferCount(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Audio, "called");

    const auto buffer_count{impl->GetSystem().GetMixBufferCount()};

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(buffer_count);
}

void IAudioRenderer::RequestUpdate(HLERequestContext& ctx) {
    LOG_TRACE(Service_Audio, "called");

    const auto input{ctx.ReadBuffer(0)};

    // These buffers are written manually to avoid an issue with WriteBuffer throwing errors for
    // checking size 0. Performance size is 0 for most games.

    auto is_buffer_b{ctx.BufferDescriptorB()[0].Size() != 0};
    if (is_buffer_b) {
        const auto buffersB{ctx.BufferDescriptorB()};
        output_buffer.resize_destructive(buffersB[0].Size());
        performance_buffer.resize_destructive(buffersB[1].Size());
    } else {
        const auto buffersC{ctx.BufferDescriptorC()};
        output_buffer.resize_destructive(buffersC[0].Size());
        performance_buffer.resize_destructive(buffersC[1].Size());
    }

    auto result = impl->RequestUpdate(input, performance_buffer, output_buffer);

    if (result.IsSuccess()) {
        if (is_buffer_b) {
            ctx.WriteBufferB(output_buffer.data(), output_buffer.size(), 0);
            ctx.WriteBufferB(performance_buffer.data(), performance_buffer.size(), 1);
        } else {
            ctx.WriteBufferC(output_buffer.data(), output_buffer.size(), 0);
            ctx.WriteBufferC(performance_buffer.data(), performance_buffer.size(), 1);
        }
    } else {
        LOG_ERROR(Service_Audio, "RequestUpdate failed error 0x{:02X}!", result.GetDescription());
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IAudioRenderer::Start(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Audio, "called");

    impl->Start();

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IAudioRenderer::Stop(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Audio, "called");

    impl->Stop();

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IAudioRenderer::QuerySystemEvent(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Audio, "called");

    if (impl->GetSystem().GetExecutionMode() == AudioCore::ExecutionMode::Manual) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(Audio::ResultNotSupported);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(rendered_event->GetReadableEvent());
}

void IAudioRenderer::SetRenderingTimeLimit(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Audio, "called");

    IPC::RequestParser rp{ctx};
    auto limit = rp.PopRaw<u32>();

    auto& system_ = impl->GetSystem();
    system_.SetRenderingTimeLimit(limit);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IAudioRenderer::GetRenderingTimeLimit(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Audio, "called");

    auto& system_ = impl->GetSystem();
    auto time = system_.GetRenderingTimeLimit();

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(time);
}

void IAudioRenderer::ExecuteAudioRendererRendering(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Audio, "called");
}

void IAudioRenderer::SetVoiceDropParameter(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Audio, "called");

    IPC::RequestParser rp{ctx};
    auto voice_drop_param{rp.Pop<f32>()};

    auto& system_ = impl->GetSystem();
    system_.SetVoiceDropParameter(voice_drop_param);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IAudioRenderer::GetVoiceDropParameter(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Audio, "called");

    auto& system_ = impl->GetSystem();
    auto voice_drop_param{system_.GetVoiceDropParameter()};

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(voice_drop_param);
}

} // namespace Service::Audio
