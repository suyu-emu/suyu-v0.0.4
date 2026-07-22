// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/cmif_types.h"
#include "core/hle/service/service.h"
#include "core/hle/service/set/settings_types.h"

namespace Core {
class System;
}

namespace Service::Set {
class ISystemSettingsServer;
}

namespace Service::Audio {

class IAudioController final : public ServiceFramework<IAudioController> {
public:
    explicit IAudioController(Core::System& system_);
    ~IAudioController() override;

private:
    enum class ForceMutePolicy {
        Disable,
        SpeakerMuteOnHeadphoneUnplugged,
    };

    enum class HeadphoneOutputLevelMode {
        Normal,
        HighPower,
    };

    Result GetTargetVolumeMin(Out<s32> out_target_min_volume);
    Result GetTargetVolumeMax(Out<s32> out_target_max_volume);
    Result GetTargetVolume(Out<s32> out_target_volume, Set::AudioOutputModeTarget target);
    Result SetTargetVolume(Set::AudioOutputModeTarget target, s32 target_volume);
    Result IsTargetMute(Out<bool> out_is_target_muted, Set::AudioOutputModeTarget target);
    Result SetTargetMute(bool is_muted, Set::AudioOutputModeTarget target);
    Result GetActiveOutputTarget(Out<Set::AudioOutputModeTarget> out_active_target);
    Result GetAudioOutputMode(Out<Set::AudioOutputMode> out_output_mode,
                              Set::AudioOutputModeTarget target);
    Result SetAudioOutputMode(Set::AudioOutputModeTarget target, Set::AudioOutputMode output_mode);
    Result GetForceMutePolicy(Out<ForceMutePolicy> out_mute_policy);
    Result GetOutputModeSetting(Out<Set::AudioOutputMode> out_output_mode,
                                Set::AudioOutputModeTarget target);
    Result SetOutputModeSetting(Set::AudioOutputModeTarget target,
                                Set::AudioOutputMode output_mode);
    Result SetHeadphoneOutputLevelMode(HeadphoneOutputLevelMode output_level_mode);
    Result GetHeadphoneOutputLevelMode(Out<HeadphoneOutputLevelMode> out_output_level_mode);
    Result NotifyHeadphoneVolumeWarningDisplayedEvent();
    Result SetSpeakerAutoMuteEnabled(bool is_speaker_auto_mute_enabled);
    Result IsSpeakerAutoMuteEnabled(Out<bool> out_is_speaker_auto_mute_enabled);
    Result AcquireTargetNotification(OutCopyHandle<Kernel::KReadableEvent> out_notification_event);
    Result Unknown5000(Out<SharedPointer<IAudioController>> out_audio_controller);

    KernelHelpers::ServiceContext service_context;

    Kernel::KEvent* notification_event;
    std::shared_ptr<Service::Set::ISystemSettingsServer> m_set_sys;
    std::array<s32, 6> m_target_volumes{{15, 15, 15, 15, 15, 15}};
    std::array<bool, 6> m_target_muted{{false, false, false, false, false, false}};
    Set::AudioOutputModeTarget m_active_target{Set::AudioOutputModeTarget::Speaker};
};

} // namespace Service::Audio
