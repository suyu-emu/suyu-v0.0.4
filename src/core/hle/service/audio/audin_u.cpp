// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/string_util.h"
#include "core/hle/service/audio/audin_u.h"
#include "core/hle/service/audio/audio_in.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::Audio {
using namespace AudioCore::AudioIn;

AudInU::AudInU(Core::System& system_)
    : ServiceFramework{system_, "audin:u"}, service_context{system_, "AudInU"},
      impl{std::make_unique<AudioCore::AudioIn::Manager>(system_)} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &AudInU::ListAudioIns, "ListAudioIns"},
        {1, &AudInU::OpenAudioIn, "OpenAudioIn"},
        {2, &AudInU::ListAudioIns, "ListAudioInsAuto"},
        {3, &AudInU::OpenAudioIn, "OpenAudioInAuto"},
        {4, &AudInU::ListAudioInsAutoFiltered, "ListAudioInsAutoFiltered"},
        {5, &AudInU::OpenAudioInProtocolSpecified, "OpenAudioInProtocolSpecified"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

AudInU::~AudInU() = default;

void AudInU::ListAudioIns(HLERequestContext& ctx) {
    using namespace AudioCore::Renderer;

    LOG_DEBUG(Service_Audio, "called");

    const auto write_count =
        static_cast<u32>(ctx.GetWriteBufferNumElements<AudioDevice::AudioDeviceName>());
    std::vector<AudioDevice::AudioDeviceName> device_names{};

    u32 out_count{0};
    if (write_count > 0) {
        out_count = impl->GetDeviceNames(device_names, write_count, false);
        ctx.WriteBuffer(device_names);
    }

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(out_count);
}

void AudInU::ListAudioInsAutoFiltered(HLERequestContext& ctx) {
    using namespace AudioCore::Renderer;

    LOG_DEBUG(Service_Audio, "called");

    const auto write_count =
        static_cast<u32>(ctx.GetWriteBufferNumElements<AudioDevice::AudioDeviceName>());
    std::vector<AudioDevice::AudioDeviceName> device_names{};

    u32 out_count{0};
    if (write_count > 0) {
        out_count = impl->GetDeviceNames(device_names, write_count, true);
        ctx.WriteBuffer(device_names);
    }

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(out_count);
}

void AudInU::OpenAudioIn(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    auto in_params{rp.PopRaw<AudioInParameter>()};
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

    std::scoped_lock l{impl->mutex};
    auto link{impl->LinkToManager()};
    if (link.IsError()) {
        LOG_ERROR(Service_Audio, "Failed to link Audio In to Audio Manager");
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

    LOG_DEBUG(Service_Audio, "Opening new AudioIn, sessionid={}, free sessions={}", new_session_id,
              impl->num_free_sessions);

    auto audio_in =
        std::make_shared<IAudioIn>(system, *impl, new_session_id, device_name, in_params,
                                   process.GetPointerUnsafe(), applet_resource_user_id);
    impl->sessions[new_session_id] = audio_in->GetImpl();
    impl->applet_resource_user_ids[new_session_id] = applet_resource_user_id;

    auto& out_system = impl->sessions[new_session_id]->GetSystem();
    AudioInParameterInternal out_params{.sample_rate = out_system.GetSampleRate(),
                                        .channel_count = out_system.GetChannelCount(),
                                        .sample_format =
                                            static_cast<u32>(out_system.GetSampleFormat()),
                                        .state = static_cast<u32>(out_system.GetState())};

    IPC::ResponseBuilder rb{ctx, 6, 0, 1};

    std::string out_name{out_system.GetName()};
    ctx.WriteBuffer(out_name);

    rb.Push(ResultSuccess);
    rb.PushRaw<AudioInParameterInternal>(out_params);
    rb.PushIpcInterface<IAudioIn>(audio_in);
}

void AudInU::OpenAudioInProtocolSpecified(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    auto protocol_specified{rp.PopRaw<u64>()};
    auto in_params{rp.PopRaw<AudioInParameter>()};
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

    std::scoped_lock l{impl->mutex};
    auto link{impl->LinkToManager()};
    if (link.IsError()) {
        LOG_ERROR(Service_Audio, "Failed to link Audio In to Audio Manager");
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

    LOG_DEBUG(Service_Audio, "Opening new AudioIn, sessionid={}, free sessions={}", new_session_id,
              impl->num_free_sessions);

    auto audio_in =
        std::make_shared<IAudioIn>(system, *impl, new_session_id, device_name, in_params,
                                   process.GetPointerUnsafe(), applet_resource_user_id);
    impl->sessions[new_session_id] = audio_in->GetImpl();
    impl->applet_resource_user_ids[new_session_id] = applet_resource_user_id;

    auto& out_system = impl->sessions[new_session_id]->GetSystem();
    AudioInParameterInternal out_params{.sample_rate = out_system.GetSampleRate(),
                                        .channel_count = out_system.GetChannelCount(),
                                        .sample_format =
                                            static_cast<u32>(out_system.GetSampleFormat()),
                                        .state = static_cast<u32>(out_system.GetState())};

    IPC::ResponseBuilder rb{ctx, 6, 0, 1};

    std::string out_name{out_system.GetName()};
    if (protocol_specified == 0) {
        if (out_system.IsUac()) {
            out_name = "UacIn";
        } else {
            out_name = "DeviceIn";
        }
    }

    ctx.WriteBuffer(out_name);

    rb.Push(ResultSuccess);
    rb.PushRaw<AudioInParameterInternal>(out_params);
    rb.PushIpcInterface<IAudioIn>(audio_in);
}

} // namespace Service::Audio
