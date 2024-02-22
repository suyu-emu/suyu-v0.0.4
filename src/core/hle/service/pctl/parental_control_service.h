// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/pctl/pctl_types.h"
#include "core/hle/service/service.h"

namespace Service::PCTL {

class IParentalControlService final : public ServiceFramework<IParentalControlService> {
public:
    explicit IParentalControlService(Core::System& system_, Capability capability_);
    ~IParentalControlService() override;

private:
    bool CheckFreeCommunicationPermissionImpl() const;
    bool ConfirmStereoVisionPermissionImpl() const;
    void SetStereoVisionRestrictionImpl(bool is_restricted);

    void Initialize(HLERequestContext& ctx);
    void CheckFreeCommunicationPermission(HLERequestContext& ctx);
    void ConfirmSnsPostPermission(HLERequestContext& ctx);
    void IsRestrictionTemporaryUnlocked(HLERequestContext& ctx);
    void ConfirmStereoVisionPermission(HLERequestContext& ctx);
    void EndFreeCommunication(HLERequestContext& ctx);
    void IsFreeCommunicationAvailable(HLERequestContext& ctx);
    void IsRestrictionEnabled(HLERequestContext& ctx);
    void GetSafetyLevel(HLERequestContext& ctx);
    void GetCurrentSettings(HLERequestContext& ctx);
    void GetFreeCommunicationApplicationListCount(HLERequestContext& ctx);
    void ConfirmStereoVisionRestrictionConfigurable(HLERequestContext& ctx);
    void IsStereoVisionPermitted(HLERequestContext& ctx);
    void IsPairingActive(HLERequestContext& ctx);
    void GetSynchronizationEvent(HLERequestContext& ctx);
    void GetPlayTimerSettings(HLERequestContext& ctx);
    void GetPlayTimerEventToRequestSuspension(HLERequestContext& ctx);
    void IsPlayTimerAlarmDisabled(HLERequestContext& ctx);
    void GetUnlinkedEvent(HLERequestContext& ctx);
    void SetStereoVisionRestriction(HLERequestContext& ctx);
    void GetStereoVisionRestriction(HLERequestContext& ctx);
    void ResetConfirmedStereoVisionPermission(HLERequestContext& ctx);

    struct States {
        u64 current_tid{};
        ApplicationInfo application_info{};
        u64 tid_from_event{};
        bool launch_time_valid{};
        bool is_suspended{};
        bool temporary_unlocked{};
        bool free_communication{};
        bool stereo_vision{};
    };

    struct ParentalControlSettings {
        bool is_stero_vision_restricted{};
        bool is_free_communication_default_on{};
        bool disabled{};
    };

    States states{};
    ParentalControlSettings settings{};
    RestrictionSettings restriction_settings{};
    std::array<char, 8> pin_code{};
    Capability capability{};

    Kernel::KEvent* synchronization_event;
    Kernel::KEvent* unlinked_event;
    Kernel::KEvent* request_suspension_event;
    KernelHelpers::ServiceContext service_context;
};

} // namespace Service::PCTL
