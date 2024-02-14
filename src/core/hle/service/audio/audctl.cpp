// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging/log.h"
#include "core/hle/service/audio/audctl.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/set/system_settings_server.h"
#include "core/hle/service/sm/sm.h"

namespace Service::Audio {

AudCtl::AudCtl(Core::System& system_) : ServiceFramework{system_, "audctl"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "GetTargetVolume"},
        {1, nullptr, "SetTargetVolume"},
        {2, &AudCtl::GetTargetVolumeMin, "GetTargetVolumeMin"},
        {3, &AudCtl::GetTargetVolumeMax, "GetTargetVolumeMax"},
        {4, nullptr, "IsTargetMute"},
        {5, nullptr, "SetTargetMute"},
        {6, nullptr, "IsTargetConnected"},
        {7, nullptr, "SetDefaultTarget"},
        {8, nullptr, "GetDefaultTarget"},
        {9, &AudCtl::GetAudioOutputMode, "GetAudioOutputMode"},
        {10, &AudCtl::SetAudioOutputMode, "SetAudioOutputMode"},
        {11, nullptr, "SetForceMutePolicy"},
        {12, &AudCtl::GetForceMutePolicy, "GetForceMutePolicy"},
        {13, &AudCtl::GetOutputModeSetting, "GetOutputModeSetting"},
        {14, &AudCtl::SetOutputModeSetting, "SetOutputModeSetting"},
        {15, nullptr, "SetOutputTarget"},
        {16, nullptr, "SetInputTargetForceEnabled"},
        {17, &AudCtl::SetHeadphoneOutputLevelMode, "SetHeadphoneOutputLevelMode"},
        {18, &AudCtl::GetHeadphoneOutputLevelMode, "GetHeadphoneOutputLevelMode"},
        {19, nullptr, "AcquireAudioVolumeUpdateEventForPlayReport"},
        {20, nullptr, "AcquireAudioOutputDeviceUpdateEventForPlayReport"},
        {21, nullptr, "GetAudioOutputTargetForPlayReport"},
        {22, nullptr, "NotifyHeadphoneVolumeWarningDisplayedEvent"},
        {23, nullptr, "SetSystemOutputMasterVolume"},
        {24, nullptr, "GetSystemOutputMasterVolume"},
        {25, nullptr, "GetAudioVolumeDataForPlayReport"},
        {26, nullptr, "UpdateHeadphoneSettings"},
        {27, nullptr, "SetVolumeMappingTableForDev"},
        {28, nullptr, "GetAudioOutputChannelCountForPlayReport"},
        {29, nullptr, "BindAudioOutputChannelCountUpdateEventForPlayReport"},
        {30, &AudCtl::SetSpeakerAutoMuteEnabled, "SetSpeakerAutoMuteEnabled"},
        {31, &AudCtl::IsSpeakerAutoMuteEnabled, "IsSpeakerAutoMuteEnabled"},
        {32, nullptr, "GetActiveOutputTarget"},
        {33, nullptr, "GetTargetDeviceInfo"},
        {34, nullptr, "AcquireTargetNotification"},
        {35, nullptr, "SetHearingProtectionSafeguardTimerRemainingTimeForDebug"},
        {36, nullptr, "GetHearingProtectionSafeguardTimerRemainingTimeForDebug"},
        {37, nullptr, "SetHearingProtectionSafeguardEnabled"},
        {38, nullptr, "IsHearingProtectionSafeguardEnabled"},
        {39, nullptr, "IsHearingProtectionSafeguardMonitoringOutputForDebug"},
        {40, nullptr, "GetSystemInformationForDebug"},
        {41, nullptr, "SetVolumeButtonLongPressTime"},
        {42, nullptr, "SetNativeVolumeForDebug"},
        {10000, nullptr, "NotifyAudioOutputTargetForPlayReport"},
        {10001, nullptr, "NotifyAudioOutputChannelCountForPlayReport"},
        {10002, nullptr, "NotifyUnsupportedUsbOutputDeviceAttachedForPlayReport"},
        {10100, nullptr, "GetAudioVolumeDataForPlayReport"},
        {10101, nullptr, "BindAudioVolumeUpdateEventForPlayReport"},
        {10102, nullptr, "BindAudioOutputTargetUpdateEventForPlayReport"},
        {10103, nullptr, "GetAudioOutputTargetForPlayReport"},
        {10104, nullptr, "GetAudioOutputChannelCountForPlayReport"},
        {10105, nullptr, "BindAudioOutputChannelCountUpdateEventForPlayReport"},
        {10106, nullptr, "GetDefaultAudioOutputTargetForPlayReport"},
        {50000, nullptr, "SetAnalogInputBoostGainForPrototyping"},
    };
    // clang-format on

    RegisterHandlers(functions);

    m_set_sys =
        system.ServiceManager().GetService<Service::Set::ISystemSettingsServer>("set:sys", true);
}

AudCtl::~AudCtl() = default;

void AudCtl::GetTargetVolumeMin(HLERequestContext& ctx) {
    LOG_DEBUG(Audio, "called.");

    // This service function is currently hardcoded on the
    // actual console to this value (as of 8.0.0).
    constexpr s32 target_min_volume = 0;

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(target_min_volume);
}

void AudCtl::GetTargetVolumeMax(HLERequestContext& ctx) {
    LOG_DEBUG(Audio, "called.");

    // This service function is currently hardcoded on the
    // actual console to this value (as of 8.0.0).
    constexpr s32 target_max_volume = 15;

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(target_max_volume);
}

void AudCtl::GetAudioOutputMode(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto target{rp.PopEnum<Set::AudioOutputModeTarget>()};

    Set::AudioOutputMode output_mode{};
    const auto result = m_set_sys->GetAudioOutputMode(&output_mode, target);

    LOG_INFO(Service_SET, "called, target={}, output_mode={}", target, output_mode);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(result);
    rb.PushEnum(output_mode);
}

void AudCtl::SetAudioOutputMode(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto target{rp.PopEnum<Set::AudioOutputModeTarget>()};
    const auto output_mode{rp.PopEnum<Set::AudioOutputMode>()};

    const auto result = m_set_sys->SetAudioOutputMode(target, output_mode);

    LOG_INFO(Service_SET, "called, target={}, output_mode={}", target, output_mode);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void AudCtl::GetForceMutePolicy(HLERequestContext& ctx) {
    LOG_WARNING(Audio, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(ForceMutePolicy::Disable);
}

void AudCtl::GetOutputModeSetting(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto target{rp.PopEnum<Set::AudioOutputModeTarget>()};

    LOG_WARNING(Audio, "(STUBBED) called, target={}", target);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(Set::AudioOutputMode::ch_7_1);
}

void AudCtl::SetOutputModeSetting(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto target{rp.PopEnum<Set::AudioOutputModeTarget>()};
    const auto output_mode{rp.PopEnum<Set::AudioOutputMode>()};

    LOG_INFO(Service_SET, "called, target={}, output_mode={}", target, output_mode);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void AudCtl::SetHeadphoneOutputLevelMode(HLERequestContext& ctx) {
    LOG_WARNING(Audio, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void AudCtl::GetHeadphoneOutputLevelMode(HLERequestContext& ctx) {
    LOG_WARNING(Audio, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(HeadphoneOutputLevelMode::Normal);
}

void AudCtl::SetSpeakerAutoMuteEnabled(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto is_speaker_auto_mute_enabled{rp.Pop<bool>()};

    LOG_WARNING(Audio, "(STUBBED) called, is_speaker_auto_mute_enabled={}",
                is_speaker_auto_mute_enabled);

    const auto result = m_set_sys->SetSpeakerAutoMuteFlag(is_speaker_auto_mute_enabled);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void AudCtl::IsSpeakerAutoMuteEnabled(HLERequestContext& ctx) {
    bool is_speaker_auto_mute_enabled{};
    const auto result = m_set_sys->GetSpeakerAutoMuteFlag(&is_speaker_auto_mute_enabled);

    LOG_WARNING(Audio, "(STUBBED) called, is_speaker_auto_mute_enabled={}",
                is_speaker_auto_mute_enabled);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(result);
    rb.Push<u8>(is_speaker_auto_mute_enabled);
}

} // namespace Service::Audio
