// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/string_util.h"
#include "core/hle/service/audio/audio_out.h"
#include "core/hle/service/audio/audout_u.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/memory.h"

namespace Service::Audio {
using namespace AudioCore::AudioOut;

AudOutU::AudOutU(Core::System& system_)
    : ServiceFramework{system_, "audout:u"}, service_context{system_, "AudOutU"},
      impl{std::make_unique<AudioCore::AudioOut::Manager>(system_)} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &AudOutU::ListAudioOuts, "ListAudioOuts"},
        {1, &AudOutU::OpenAudioOut, "OpenAudioOut"},
        {2, &AudOutU::ListAudioOuts, "ListAudioOutsAuto"},
        {3, &AudOutU::OpenAudioOut, "OpenAudioOutAuto"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

AudOutU::~AudOutU() = default;

void AudOutU::ListAudioOuts(HLERequestContext& ctx) {
    using namespace AudioCore::Renderer;

    std::scoped_lock l{impl->mutex};

    const auto write_count =
        static_cast<u32>(ctx.GetWriteBufferNumElements<AudioDevice::AudioDeviceName>());
    std::vector<AudioDevice::AudioDeviceName> device_names{};
    if (write_count > 0) {
        device_names.emplace_back("DeviceOut");
        LOG_DEBUG(Service_Audio, "called. \nName=DeviceOut");
    } else {
        LOG_DEBUG(Service_Audio, "called. Empty buffer passed in.");
    }

    ctx.WriteBuffer(device_names);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u32>(static_cast<u32>(device_names.size()));
}

void AudOutU::OpenAudioOut(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    auto in_params{rp.PopRaw<AudioOutParameter>()};
    auto applet_resource_user_id{rp.PopRaw<u64>()};
    const auto device_name_data{ctx.ReadBuffer()};
    auto device_name = Common::StringFromBuffer(device_name_data);
    auto handle{ctx.GetCopyHandle(0)};

    auto process{ctx.GetObjectFromHandle<Kernel::KProcess>(handle)};
    if (process.IsNull()) {
        LOG_ERROR(Service_Audio, "Failed to get process handle");
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultUnknown);
        return;
    }

    auto link{impl->LinkToManager()};
    if (link.IsError()) {
        LOG_ERROR(Service_Audio, "Failed to link Audio Out to Audio Manager");
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(link);
        return;
    }

    size_t new_session_id{};
    auto result{impl->AcquireSessionId(new_session_id)};
    if (result.IsError()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
        return;
    }

    LOG_DEBUG(Service_Audio, "Opening new AudioOut, sessionid={}, free sessions={}", new_session_id,
              impl->num_free_sessions);

    auto audio_out =
        std::make_shared<IAudioOut>(system, *impl, new_session_id, device_name, in_params,
                                    process.GetPointerUnsafe(), applet_resource_user_id);
    result = audio_out->GetImpl()->GetSystem().Initialize(
        device_name, in_params, process.GetPointerUnsafe(), applet_resource_user_id);
    if (result.IsError()) {
        LOG_ERROR(Service_Audio, "Failed to initialize the AudioOut System!");
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
        return;
    }

    impl->sessions[new_session_id] = audio_out->GetImpl();
    impl->applet_resource_user_ids[new_session_id] = applet_resource_user_id;

    auto& out_system = impl->sessions[new_session_id]->GetSystem();
    AudioOutParameterInternal out_params{.sample_rate = out_system.GetSampleRate(),
                                         .channel_count = out_system.GetChannelCount(),
                                         .sample_format =
                                             static_cast<u32>(out_system.GetSampleFormat()),
                                         .state = static_cast<u32>(out_system.GetState())};

    IPC::ResponseBuilder rb{ctx, 6, 0, 1};

    ctx.WriteBuffer(out_system.GetName());

    rb.Push(ResultSuccess);
    rb.PushRaw<AudioOutParameterInternal>(out_params);
    rb.PushIpcInterface<IAudioOut>(audio_out);
}

} // namespace Service::Audio
