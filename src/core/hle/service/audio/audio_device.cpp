// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "audio_core/audio_core.h"
#include "common/string_util.h"
#include "core/hle/service/audio/audio_device.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::Audio {
using namespace AudioCore::Renderer;

IAudioDevice::IAudioDevice(Core::System& system_, u64 applet_resource_user_id, u32 revision,
                           u32 device_num)
    : ServiceFramework{system_, "IAudioDevice"}, service_context{system_, "IAudioDevice"},
      impl{std::make_unique<AudioDevice>(system_, applet_resource_user_id, revision)},
      event{service_context.CreateEvent(fmt::format("IAudioDeviceEvent-{}", device_num))} {
    static const FunctionInfo functions[] = {
        {0, &IAudioDevice::ListAudioDeviceName, "ListAudioDeviceName"},
        {1, &IAudioDevice::SetAudioDeviceOutputVolume, "SetAudioDeviceOutputVolume"},
        {2, &IAudioDevice::GetAudioDeviceOutputVolume, "GetAudioDeviceOutputVolume"},
        {3, &IAudioDevice::GetActiveAudioDeviceName, "GetActiveAudioDeviceName"},
        {4, &IAudioDevice::QueryAudioDeviceSystemEvent, "QueryAudioDeviceSystemEvent"},
        {5, &IAudioDevice::GetActiveChannelCount, "GetActiveChannelCount"},
        {6, &IAudioDevice::ListAudioDeviceName, "ListAudioDeviceNameAuto"},
        {7, &IAudioDevice::SetAudioDeviceOutputVolume, "SetAudioDeviceOutputVolumeAuto"},
        {8, &IAudioDevice::GetAudioDeviceOutputVolume, "GetAudioDeviceOutputVolumeAuto"},
        {10, &IAudioDevice::GetActiveAudioDeviceName, "GetActiveAudioDeviceNameAuto"},
        {11, &IAudioDevice::QueryAudioDeviceInputEvent, "QueryAudioDeviceInputEvent"},
        {12, &IAudioDevice::QueryAudioDeviceOutputEvent, "QueryAudioDeviceOutputEvent"},
        {13, &IAudioDevice::GetActiveAudioDeviceName, "GetActiveAudioOutputDeviceName"},
        {14, &IAudioDevice::ListAudioOutputDeviceName, "ListAudioOutputDeviceName"},
    };
    RegisterHandlers(functions);

    event->Signal();
}

IAudioDevice::~IAudioDevice() {
    service_context.CloseEvent(event);
}

void IAudioDevice::ListAudioDeviceName(HLERequestContext& ctx) {
    const size_t in_count = ctx.GetWriteBufferNumElements<AudioDevice::AudioDeviceName>();

    std::vector<AudioDevice::AudioDeviceName> out_names{};

    const u32 out_count = impl->ListAudioDeviceName(out_names, in_count);

    std::string out{};
    for (u32 i = 0; i < out_count; i++) {
        std::string a{};
        u32 j = 0;
        while (out_names[i].name[j] != '\0') {
            a += out_names[i].name[j];
            j++;
        }
        out += "\n\t" + a;
    }

    LOG_DEBUG(Service_Audio, "called.\nNames={}", out);

    IPC::ResponseBuilder rb{ctx, 3};

    ctx.WriteBuffer(out_names);

    rb.Push(ResultSuccess);
    rb.Push(out_count);
}

void IAudioDevice::SetAudioDeviceOutputVolume(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const f32 volume = rp.Pop<f32>();

    const auto device_name_buffer = ctx.ReadBuffer();
    const std::string name = Common::StringFromBuffer(device_name_buffer);

    LOG_DEBUG(Service_Audio, "called. name={}, volume={}", name, volume);

    if (name == "AudioTvOutput") {
        impl->SetDeviceVolumes(volume);
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IAudioDevice::GetAudioDeviceOutputVolume(HLERequestContext& ctx) {
    const auto device_name_buffer = ctx.ReadBuffer();
    const std::string name = Common::StringFromBuffer(device_name_buffer);

    LOG_DEBUG(Service_Audio, "called. Name={}", name);

    f32 volume{1.0f};
    if (name == "AudioTvOutput") {
        volume = impl->GetDeviceVolume(name);
    }

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(volume);
}

void IAudioDevice::GetActiveAudioDeviceName(HLERequestContext& ctx) {
    const auto write_size = ctx.GetWriteBufferSize();
    std::string out_name{"AudioTvOutput"};

    LOG_DEBUG(Service_Audio, "(STUBBED) called. Name={}", out_name);

    out_name.resize(write_size);

    ctx.WriteBuffer(out_name);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IAudioDevice::QueryAudioDeviceSystemEvent(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Audio, "(STUBBED) called");

    event->Signal();

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(event->GetReadableEvent());
}

void IAudioDevice::GetActiveChannelCount(HLERequestContext& ctx) {
    const auto& sink{system.AudioCore().GetOutputSink()};
    u32 channel_count{sink.GetSystemChannels()};

    LOG_DEBUG(Service_Audio, "(STUBBED) called. Channels={}", channel_count);

    IPC::ResponseBuilder rb{ctx, 3};

    rb.Push(ResultSuccess);
    rb.Push<u32>(channel_count);
}

void IAudioDevice::QueryAudioDeviceInputEvent(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Audio, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(event->GetReadableEvent());
}

void IAudioDevice::QueryAudioDeviceOutputEvent(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Audio, "called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(event->GetReadableEvent());
}

void IAudioDevice::ListAudioOutputDeviceName(HLERequestContext& ctx) {
    const size_t in_count = ctx.GetWriteBufferNumElements<AudioDevice::AudioDeviceName>();

    std::vector<AudioDevice::AudioDeviceName> out_names{};

    const u32 out_count = impl->ListAudioOutputDeviceName(out_names, in_count);

    std::string out{};
    for (u32 i = 0; i < out_count; i++) {
        std::string a{};
        u32 j = 0;
        while (out_names[i].name[j] != '\0') {
            a += out_names[i].name[j];
            j++;
        }
        out += "\n\t" + a;
    }

    LOG_DEBUG(Service_Audio, "called.\nNames={}", out);

    IPC::ResponseBuilder rb{ctx, 3};

    ctx.WriteBuffer(out_names);

    rb.Push(ResultSuccess);
    rb.Push(out_count);
}

} // namespace Service::Audio
