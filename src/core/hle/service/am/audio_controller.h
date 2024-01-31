// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Service::AM {

class IAudioController final : public ServiceFramework<IAudioController> {
public:
    explicit IAudioController(Core::System& system_);
    ~IAudioController() override;

private:
    void SetExpectedMasterVolume(HLERequestContext& ctx);
    void GetMainAppletExpectedMasterVolume(HLERequestContext& ctx);
    void GetLibraryAppletExpectedMasterVolume(HLERequestContext& ctx);
    void ChangeMainAppletMasterVolume(HLERequestContext& ctx);
    void SetTransparentAudioRate(HLERequestContext& ctx);

    static constexpr float min_allowed_volume = 0.0f;
    static constexpr float max_allowed_volume = 1.0f;

    float main_applet_volume{0.25f};
    float library_applet_volume{max_allowed_volume};
    float transparent_volume_rate{min_allowed_volume};

    // Volume transition fade time in nanoseconds.
    // e.g. If the main applet volume was 0% and was changed to 50%
    //      with a fade of 50ns, then over the course of 50ns,
    //      the volume will gradually fade up to 50%
    std::chrono::nanoseconds fade_time_ns{0};
};

} // namespace Service::AM
