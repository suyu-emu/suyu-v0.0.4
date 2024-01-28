// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::Set {
class ISystemSettingsServer;
}

namespace Service::Audio {

class AudCtl final : public ServiceFramework<AudCtl> {
public:
    explicit AudCtl(Core::System& system_);
    ~AudCtl() override;

private:
    enum class ForceMutePolicy {
        Disable,
        SpeakerMuteOnHeadphoneUnplugged,
    };

    enum class HeadphoneOutputLevelMode {
        Normal,
        HighPower,
    };

    void GetTargetVolumeMin(HLERequestContext& ctx);
    void GetTargetVolumeMax(HLERequestContext& ctx);
    void GetAudioOutputMode(HLERequestContext& ctx);
    void SetAudioOutputMode(HLERequestContext& ctx);
    void GetForceMutePolicy(HLERequestContext& ctx);
    void GetOutputModeSetting(HLERequestContext& ctx);
    void SetOutputModeSetting(HLERequestContext& ctx);
    void SetHeadphoneOutputLevelMode(HLERequestContext& ctx);
    void GetHeadphoneOutputLevelMode(HLERequestContext& ctx);
    void SetSpeakerAutoMuteEnabled(HLERequestContext& ctx);
    void IsSpeakerAutoMuteEnabled(HLERequestContext& ctx);
    void AcquireTargetNotification(HLERequestContext& ctx);

    std::shared_ptr<Service::Set::ISystemSettingsServer> m_set_sys;
};

} // namespace Service::Audio
