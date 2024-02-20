// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "audio_core/audio_render_manager.h"
#include "audio_core/common/feature_support.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_transfer_memory.h"
#include "core/hle/service/audio/audio_device.h"
#include "core/hle/service/audio/audio_renderer.h"
#include "core/hle/service/audio/audio_renderer_manager.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::Audio {

using namespace AudioCore::Renderer;

IAudioRendererManager::IAudioRendererManager(Core::System& system_)
    : ServiceFramework{system_, "audren:u"}, service_context{system_, "audren:u"},
      impl{std::make_unique<Manager>(system_)} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &IAudioRendererManager::OpenAudioRenderer, "OpenAudioRenderer"},
        {1, &IAudioRendererManager::GetWorkBufferSize, "GetWorkBufferSize"},
        {2, &IAudioRendererManager::GetAudioDeviceService, "GetAudioDeviceService"},
        {3, nullptr, "OpenAudioRendererForManualExecution"},
        {4, &IAudioRendererManager::GetAudioDeviceServiceWithRevisionInfo, "GetAudioDeviceServiceWithRevisionInfo"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

IAudioRendererManager::~IAudioRendererManager() = default;

void IAudioRendererManager::OpenAudioRenderer(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    AudioCore::AudioRendererParameterInternal params;
    rp.PopRaw<AudioCore::AudioRendererParameterInternal>(params);
    rp.Skip(1, false);
    auto transfer_memory_size = rp.Pop<u64>();
    auto applet_resource_user_id = rp.Pop<u64>();
    auto transfer_memory_handle = ctx.GetCopyHandle(0);
    auto process_handle = ctx.GetCopyHandle(1);

    if (impl->GetSessionCount() + 1 > AudioCore::MaxRendererSessions) {
        LOG_ERROR(Service_Audio, "Too many AudioRenderer sessions open!");
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(Audio::ResultOutOfSessions);
        return;
    }

    auto process{ctx.GetObjectFromHandle<Kernel::KProcess>(process_handle).GetPointerUnsafe()};
    auto transfer_memory{ctx.GetObjectFromHandle<Kernel::KTransferMemory>(transfer_memory_handle)};

    const auto session_id{impl->GetSessionId()};
    if (session_id == -1) {
        LOG_ERROR(Service_Audio, "Tried to open a session that's already in use!");
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(Audio::ResultOutOfSessions);
        return;
    }

    LOG_DEBUG(Service_Audio, "Opened new AudioRenderer session {} sessions open {}", session_id,
              impl->GetSessionCount());

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IAudioRenderer>(system, *impl, params, transfer_memory.GetPointerUnsafe(),
                                        transfer_memory_size, process_handle, *process,
                                        applet_resource_user_id, session_id);
}

void IAudioRendererManager::GetWorkBufferSize(HLERequestContext& ctx) {
    AudioCore::AudioRendererParameterInternal params;

    IPC::RequestParser rp{ctx};
    rp.PopRaw<AudioCore::AudioRendererParameterInternal>(params);

    u64 size{0};
    auto result = impl->GetWorkBufferSize(params, size);

    std::string output_info{};
    output_info += fmt::format("\tRevision {}", AudioCore::GetRevisionNum(params.revision));
    output_info +=
        fmt::format("\n\tSample Rate {}, Sample Count {}", params.sample_rate, params.sample_count);
    output_info += fmt::format("\n\tExecution Mode {}, Voice Drop Enabled {}",
                               static_cast<u32>(params.execution_mode), params.voice_drop_enabled);
    output_info += fmt::format(
        "\n\tSizes: Effects {:04X}, Mixes {:04X}, Sinks {:04X}, Submixes {:04X}, Splitter Infos "
        "{:04X}, Splitter Destinations {:04X}, Voices {:04X}, Performance Frames {:04X} External "
        "Context {:04X}",
        params.effects, params.mixes, params.sinks, params.sub_mixes, params.splitter_infos,
        params.splitter_destinations, params.voices, params.perf_frames,
        params.external_context_size);

    LOG_DEBUG(Service_Audio, "called.\nInput params:\n{}\nOutput params:\n\tWorkbuffer size {:08X}",
              output_info, size);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(result);
    rb.Push<u64>(size);
}

void IAudioRendererManager::GetAudioDeviceService(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    const auto applet_resource_user_id = rp.Pop<u64>();

    LOG_DEBUG(Service_Audio, "called. Applet resource id {}", applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};

    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IAudioDevice>(system, applet_resource_user_id,
                                      ::Common::MakeMagic('R', 'E', 'V', '1'), num_audio_devices++);
}

void IAudioRendererManager::OpenAudioRendererForManualExecution(HLERequestContext& ctx) {
    LOG_ERROR(Service_Audio, "called. Implement me!");
}

void IAudioRendererManager::GetAudioDeviceServiceWithRevisionInfo(HLERequestContext& ctx) {
    struct Parameters {
        u32 revision;
        u64 applet_resource_user_id;
    };

    IPC::RequestParser rp{ctx};

    const auto [revision, applet_resource_user_id] = rp.PopRaw<Parameters>();

    LOG_DEBUG(Service_Audio, "called. Revision {} Applet resource id {}",
              AudioCore::GetRevisionNum(revision), applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};

    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IAudioDevice>(system, applet_resource_user_id, revision,
                                      num_audio_devices++);
}

} // namespace Service::Audio
