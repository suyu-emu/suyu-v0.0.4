// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/am/audio_controller.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::AM {

IAudioController::IAudioController(Core::System& system_)
    : ServiceFramework{system_, "IAudioController"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &IAudioController::SetExpectedMasterVolume, "SetExpectedMasterVolume"},
        {1, &IAudioController::GetMainAppletExpectedMasterVolume, "GetMainAppletExpectedMasterVolume"},
        {2, &IAudioController::GetLibraryAppletExpectedMasterVolume, "GetLibraryAppletExpectedMasterVolume"},
        {3, &IAudioController::ChangeMainAppletMasterVolume, "ChangeMainAppletMasterVolume"},
        {4, &IAudioController::SetTransparentAudioRate, "SetTransparentVolumeRate"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

IAudioController::~IAudioController() = default;

void IAudioController::SetExpectedMasterVolume(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const float main_applet_volume_tmp = rp.Pop<float>();
    const float library_applet_volume_tmp = rp.Pop<float>();

    LOG_DEBUG(Service_AM, "called. main_applet_volume={}, library_applet_volume={}",
              main_applet_volume_tmp, library_applet_volume_tmp);

    // Ensure the volume values remain within the 0-100% range
    main_applet_volume = std::clamp(main_applet_volume_tmp, min_allowed_volume, max_allowed_volume);
    library_applet_volume =
        std::clamp(library_applet_volume_tmp, min_allowed_volume, max_allowed_volume);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IAudioController::GetMainAppletExpectedMasterVolume(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called. main_applet_volume={}", main_applet_volume);
    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(main_applet_volume);
}

void IAudioController::GetLibraryAppletExpectedMasterVolume(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called. library_applet_volume={}", library_applet_volume);
    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(library_applet_volume);
}

void IAudioController::ChangeMainAppletMasterVolume(HLERequestContext& ctx) {
    struct Parameters {
        float volume;
        s64 fade_time_ns;
    };
    static_assert(sizeof(Parameters) == 16);

    IPC::RequestParser rp{ctx};
    const auto parameters = rp.PopRaw<Parameters>();

    LOG_DEBUG(Service_AM, "called. volume={}, fade_time_ns={}", parameters.volume,
              parameters.fade_time_ns);

    main_applet_volume = std::clamp(parameters.volume, min_allowed_volume, max_allowed_volume);
    fade_time_ns = std::chrono::nanoseconds{parameters.fade_time_ns};

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IAudioController::SetTransparentAudioRate(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const float transparent_volume_rate_tmp = rp.Pop<float>();

    LOG_DEBUG(Service_AM, "called. transparent_volume_rate={}", transparent_volume_rate_tmp);

    // Clamp volume range to 0-100%.
    transparent_volume_rate =
        std::clamp(transparent_volume_rate_tmp, min_allowed_volume, max_allowed_volume);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

} // namespace Service::AM
