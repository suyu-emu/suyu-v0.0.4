// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/patch_manager.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/pctl/parental_control_service.h"
#include "core/hle/service/pctl/pctl_results.h"

namespace Service::PCTL {

IParentalControlService::IParentalControlService(Core::System& system_, Capability capability_)
    : ServiceFramework{system_, "IParentalControlService"}, capability{capability_},
      service_context{system_, "IParentalControlService"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {1, &IParentalControlService::Initialize, "Initialize"},
        {1001, &IParentalControlService::CheckFreeCommunicationPermission, "CheckFreeCommunicationPermission"},
        {1002, nullptr, "ConfirmLaunchApplicationPermission"},
        {1003, nullptr, "ConfirmResumeApplicationPermission"},
        {1004, &IParentalControlService::ConfirmSnsPostPermission, "ConfirmSnsPostPermission"},
        {1005, nullptr, "ConfirmSystemSettingsPermission"},
        {1006, &IParentalControlService::IsRestrictionTemporaryUnlocked, "IsRestrictionTemporaryUnlocked"},
        {1007, nullptr, "RevertRestrictionTemporaryUnlocked"},
        {1008, nullptr, "EnterRestrictedSystemSettings"},
        {1009, nullptr, "LeaveRestrictedSystemSettings"},
        {1010, nullptr, "IsRestrictedSystemSettingsEntered"},
        {1011, nullptr, "RevertRestrictedSystemSettingsEntered"},
        {1012, nullptr, "GetRestrictedFeatures"},
        {1013, &IParentalControlService::ConfirmStereoVisionPermission, "ConfirmStereoVisionPermission"},
        {1014, nullptr, "ConfirmPlayableApplicationVideoOld"},
        {1015, nullptr, "ConfirmPlayableApplicationVideo"},
        {1016, nullptr, "ConfirmShowNewsPermission"},
        {1017, &IParentalControlService::EndFreeCommunication, "EndFreeCommunication"},
        {1018, &IParentalControlService::IsFreeCommunicationAvailable, "IsFreeCommunicationAvailable"},
        {1031, &IParentalControlService::IsRestrictionEnabled, "IsRestrictionEnabled"},
        {1032, &IParentalControlService::GetSafetyLevel, "GetSafetyLevel"},
        {1033, nullptr, "SetSafetyLevel"},
        {1034, nullptr, "GetSafetyLevelSettings"},
        {1035, &IParentalControlService::GetCurrentSettings, "GetCurrentSettings"},
        {1036, nullptr, "SetCustomSafetyLevelSettings"},
        {1037, nullptr, "GetDefaultRatingOrganization"},
        {1038, nullptr, "SetDefaultRatingOrganization"},
        {1039, &IParentalControlService::GetFreeCommunicationApplicationListCount, "GetFreeCommunicationApplicationListCount"},
        {1042, nullptr, "AddToFreeCommunicationApplicationList"},
        {1043, nullptr, "DeleteSettings"},
        {1044, nullptr, "GetFreeCommunicationApplicationList"},
        {1045, nullptr, "UpdateFreeCommunicationApplicationList"},
        {1046, nullptr, "DisableFeaturesForReset"},
        {1047, nullptr, "NotifyApplicationDownloadStarted"},
        {1048, nullptr, "NotifyNetworkProfileCreated"},
        {1049, nullptr, "ResetFreeCommunicationApplicationList"},
        {1061, &IParentalControlService::ConfirmStereoVisionRestrictionConfigurable, "ConfirmStereoVisionRestrictionConfigurable"},
        {1062, &IParentalControlService::GetStereoVisionRestriction, "GetStereoVisionRestriction"},
        {1063, &IParentalControlService::SetStereoVisionRestriction, "SetStereoVisionRestriction"},
        {1064, &IParentalControlService::ResetConfirmedStereoVisionPermission, "ResetConfirmedStereoVisionPermission"},
        {1065, &IParentalControlService::IsStereoVisionPermitted, "IsStereoVisionPermitted"},
        {1201, nullptr, "UnlockRestrictionTemporarily"},
        {1202, nullptr, "UnlockSystemSettingsRestriction"},
        {1203, nullptr, "SetPinCode"},
        {1204, nullptr, "GenerateInquiryCode"},
        {1205, nullptr, "CheckMasterKey"},
        {1206, nullptr, "GetPinCodeLength"},
        {1207, nullptr, "GetPinCodeChangedEvent"},
        {1208, nullptr, "GetPinCode"},
        {1403, &IParentalControlService::IsPairingActive, "IsPairingActive"},
        {1406, nullptr, "GetSettingsLastUpdated"},
        {1411, nullptr, "GetPairingAccountInfo"},
        {1421, nullptr, "GetAccountNickname"},
        {1424, nullptr, "GetAccountState"},
        {1425, nullptr, "RequestPostEvents"},
        {1426, nullptr, "GetPostEventInterval"},
        {1427, nullptr, "SetPostEventInterval"},
        {1432, &IParentalControlService::GetSynchronizationEvent, "GetSynchronizationEvent"},
        {1451, nullptr, "StartPlayTimer"},
        {1452, nullptr, "StopPlayTimer"},
        {1453, nullptr, "IsPlayTimerEnabled"},
        {1454, nullptr, "GetPlayTimerRemainingTime"},
        {1455, nullptr, "IsRestrictedByPlayTimer"},
        {1456, &IParentalControlService::GetPlayTimerSettings, "GetPlayTimerSettings"},
        {1457, &IParentalControlService::GetPlayTimerEventToRequestSuspension, "GetPlayTimerEventToRequestSuspension"},
        {1458, &IParentalControlService::IsPlayTimerAlarmDisabled, "IsPlayTimerAlarmDisabled"},
        {1471, nullptr, "NotifyWrongPinCodeInputManyTimes"},
        {1472, nullptr, "CancelNetworkRequest"},
        {1473, &IParentalControlService::GetUnlinkedEvent, "GetUnlinkedEvent"},
        {1474, nullptr, "ClearUnlinkedEvent"},
        {1601, nullptr, "DisableAllFeatures"},
        {1602, nullptr, "PostEnableAllFeatures"},
        {1603, nullptr, "IsAllFeaturesDisabled"},
        {1901, nullptr, "DeleteFromFreeCommunicationApplicationListForDebug"},
        {1902, nullptr, "ClearFreeCommunicationApplicationListForDebug"},
        {1903, nullptr, "GetExemptApplicationListCountForDebug"},
        {1904, nullptr, "GetExemptApplicationListForDebug"},
        {1905, nullptr, "UpdateExemptApplicationListForDebug"},
        {1906, nullptr, "AddToExemptApplicationListForDebug"},
        {1907, nullptr, "DeleteFromExemptApplicationListForDebug"},
        {1908, nullptr, "ClearExemptApplicationListForDebug"},
        {1941, nullptr, "DeletePairing"},
        {1951, nullptr, "SetPlayTimerSettingsForDebug"},
        {1952, nullptr, "GetPlayTimerSpentTimeForTest"},
        {1953, nullptr, "SetPlayTimerAlarmDisabledForDebug"},
        {2001, nullptr, "RequestPairingAsync"},
        {2002, nullptr, "FinishRequestPairing"},
        {2003, nullptr, "AuthorizePairingAsync"},
        {2004, nullptr, "FinishAuthorizePairing"},
        {2005, nullptr, "RetrievePairingInfoAsync"},
        {2006, nullptr, "FinishRetrievePairingInfo"},
        {2007, nullptr, "UnlinkPairingAsync"},
        {2008, nullptr, "FinishUnlinkPairing"},
        {2009, nullptr, "GetAccountMiiImageAsync"},
        {2010, nullptr, "FinishGetAccountMiiImage"},
        {2011, nullptr, "GetAccountMiiImageContentTypeAsync"},
        {2012, nullptr, "FinishGetAccountMiiImageContentType"},
        {2013, nullptr, "SynchronizeParentalControlSettingsAsync"},
        {2014, nullptr, "FinishSynchronizeParentalControlSettings"},
        {2015, nullptr, "FinishSynchronizeParentalControlSettingsWithLastUpdated"},
        {2016, nullptr, "RequestUpdateExemptionListAsync"},
    };
    // clang-format on
    RegisterHandlers(functions);

    synchronization_event =
        service_context.CreateEvent("IParentalControlService::SynchronizationEvent");
    unlinked_event = service_context.CreateEvent("IParentalControlService::UnlinkedEvent");
    request_suspension_event =
        service_context.CreateEvent("IParentalControlService::RequestSuspensionEvent");
}

IParentalControlService::~IParentalControlService() {
    service_context.CloseEvent(synchronization_event);
    service_context.CloseEvent(unlinked_event);
    service_context.CloseEvent(request_suspension_event);
}

bool IParentalControlService::CheckFreeCommunicationPermissionImpl() const {
    if (states.temporary_unlocked) {
        return true;
    }
    if ((states.application_info.parental_control_flag & 1) == 0) {
        return true;
    }
    if (pin_code[0] == '\0') {
        return true;
    }
    if (!settings.is_free_communication_default_on) {
        return true;
    }
    // TODO(ogniK): Check for blacklisted/exempted applications. Return false can happen here
    // but as we don't have multiproceses support yet, we can just assume our application is
    // valid for the time being
    return true;
}

bool IParentalControlService::ConfirmStereoVisionPermissionImpl() const {
    if (states.temporary_unlocked) {
        return true;
    }
    if (pin_code[0] == '\0') {
        return true;
    }
    if (!settings.is_stero_vision_restricted) {
        return false;
    }
    return true;
}

void IParentalControlService::SetStereoVisionRestrictionImpl(bool is_restricted) {
    if (settings.disabled) {
        return;
    }

    if (pin_code[0] == '\0') {
        return;
    }
    settings.is_stero_vision_restricted = is_restricted;
}

void IParentalControlService::Initialize(HLERequestContext& ctx) {
    LOG_DEBUG(Service_PCTL, "called");
    IPC::ResponseBuilder rb{ctx, 2};

    if (False(capability & (Capability::Application | Capability::System))) {
        LOG_ERROR(Service_PCTL, "Invalid capability! capability={:X}", capability);
        return;
    }

    // TODO(ogniK): Recovery flag initialization for pctl:r

    const auto tid = system.GetApplicationProcessProgramID();
    if (tid != 0) {
        const FileSys::PatchManager pm{tid, system.GetFileSystemController(),
                                       system.GetContentProvider()};
        const auto control = pm.GetControlMetadata();
        if (control.first) {
            states.tid_from_event = 0;
            states.launch_time_valid = false;
            states.is_suspended = false;
            states.free_communication = false;
            states.stereo_vision = false;
            states.application_info = ApplicationInfo{
                .application_id = tid,
                .age_rating = control.first->GetRatingAge(),
                .parental_control_flag = control.first->GetParentalControlFlag(),
                .capability = capability,
            };

            if (False(capability & (Capability::System | Capability::Recovery))) {
                // TODO(ogniK): Signal application launch event
            }
        }
    }

    rb.Push(ResultSuccess);
}

void IParentalControlService::CheckFreeCommunicationPermission(HLERequestContext& ctx) {
    LOG_DEBUG(Service_PCTL, "called");

    IPC::ResponseBuilder rb{ctx, 2};
    if (!CheckFreeCommunicationPermissionImpl()) {
        rb.Push(PCTL::ResultNoFreeCommunication);
    } else {
        rb.Push(ResultSuccess);
    }

    states.free_communication = true;
}

void IParentalControlService::ConfirmSnsPostPermission(HLERequestContext& ctx) {
    LOG_WARNING(Service_PCTL, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(PCTL::ResultNoFreeCommunication);
}

void IParentalControlService::IsRestrictionTemporaryUnlocked(HLERequestContext& ctx) {
    const bool is_temporary_unlocked = false;

    LOG_WARNING(Service_PCTL, "(STUBBED) called, is_temporary_unlocked={}", is_temporary_unlocked);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u8>(is_temporary_unlocked);
}

void IParentalControlService::ConfirmStereoVisionPermission(HLERequestContext& ctx) {
    LOG_DEBUG(Service_PCTL, "called");
    states.stereo_vision = true;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IParentalControlService::EndFreeCommunication(HLERequestContext& ctx) {
    LOG_WARNING(Service_PCTL, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IParentalControlService::IsFreeCommunicationAvailable(HLERequestContext& ctx) {
    LOG_WARNING(Service_PCTL, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    if (!CheckFreeCommunicationPermissionImpl()) {
        rb.Push(PCTL::ResultNoFreeCommunication);
    } else {
        rb.Push(ResultSuccess);
    }
}

void IParentalControlService::IsRestrictionEnabled(HLERequestContext& ctx) {
    LOG_DEBUG(Service_PCTL, "called");

    IPC::ResponseBuilder rb{ctx, 3};
    if (False(capability & (Capability::Status | Capability::Recovery))) {
        LOG_ERROR(Service_PCTL, "Application does not have Status or Recovery capabilities!");
        rb.Push(PCTL::ResultNoCapability);
        rb.Push(false);
        return;
    }

    rb.Push(pin_code[0] != '\0');
}

void IParentalControlService::GetSafetyLevel(HLERequestContext& ctx) {
    const u32 safety_level = 0;

    LOG_WARNING(Service_PCTL, "(STUBBED) called, safety_level={}", safety_level);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(safety_level);
}

void IParentalControlService::GetCurrentSettings(HLERequestContext& ctx) {
    LOG_INFO(Service_PCTL, "called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushRaw(restriction_settings);
}

void IParentalControlService::GetFreeCommunicationApplicationListCount(HLERequestContext& ctx) {
    const u32 count = 4;

    LOG_WARNING(Service_PCTL, "(STUBBED) called, count={}", count);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(count);
}

void IParentalControlService::ConfirmStereoVisionRestrictionConfigurable(HLERequestContext& ctx) {
    LOG_DEBUG(Service_PCTL, "called");

    IPC::ResponseBuilder rb{ctx, 2};

    if (False(capability & Capability::StereoVision)) {
        LOG_ERROR(Service_PCTL, "Application does not have StereoVision capability!");
        rb.Push(PCTL::ResultNoCapability);
        return;
    }

    if (pin_code[0] == '\0') {
        rb.Push(PCTL::ResultNoRestrictionEnabled);
        return;
    }

    rb.Push(ResultSuccess);
}

void IParentalControlService::IsStereoVisionPermitted(HLERequestContext& ctx) {
    LOG_DEBUG(Service_PCTL, "called");

    IPC::ResponseBuilder rb{ctx, 3};
    if (!ConfirmStereoVisionPermissionImpl()) {
        rb.Push(PCTL::ResultStereoVisionRestricted);
        rb.Push(false);
    } else {
        rb.Push(ResultSuccess);
        rb.Push(true);
    }
}

void IParentalControlService::IsPairingActive(HLERequestContext& ctx) {
    const bool is_pairing_active = false;

    LOG_WARNING(Service_PCTL, "(STUBBED) called, is_pairing_active={}", is_pairing_active);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u8>(is_pairing_active);
}

void IParentalControlService::GetSynchronizationEvent(HLERequestContext& ctx) {
    LOG_INFO(Service_PCTL, "called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(synchronization_event->GetReadableEvent());
}

void IParentalControlService::GetPlayTimerSettings(HLERequestContext& ctx) {
    LOG_WARNING(Service_PCTL, "(STUBBED) called");

    const PlayTimerSettings timer_settings{};

    IPC::ResponseBuilder rb{ctx, 15};
    rb.Push(ResultSuccess);
    rb.PushRaw(timer_settings);
}

void IParentalControlService::GetPlayTimerEventToRequestSuspension(HLERequestContext& ctx) {
    LOG_INFO(Service_PCTL, "called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(request_suspension_event->GetReadableEvent());
}

void IParentalControlService::IsPlayTimerAlarmDisabled(HLERequestContext& ctx) {
    const bool is_play_timer_alarm_disabled = false;

    LOG_INFO(Service_PCTL, "called, is_play_timer_alarm_disabled={}", is_play_timer_alarm_disabled);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u8>(is_play_timer_alarm_disabled);
}

void IParentalControlService::GetUnlinkedEvent(HLERequestContext& ctx) {
    LOG_INFO(Service_PCTL, "called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(unlinked_event->GetReadableEvent());
}

void IParentalControlService::SetStereoVisionRestriction(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto can_use = rp.Pop<bool>();
    LOG_DEBUG(Service_PCTL, "called, can_use={}", can_use);

    IPC::ResponseBuilder rb{ctx, 2};
    if (False(capability & Capability::StereoVision)) {
        LOG_ERROR(Service_PCTL, "Application does not have StereoVision capability!");
        rb.Push(PCTL::ResultNoCapability);
        return;
    }

    SetStereoVisionRestrictionImpl(can_use);
    rb.Push(ResultSuccess);
}

void IParentalControlService::GetStereoVisionRestriction(HLERequestContext& ctx) {
    LOG_DEBUG(Service_PCTL, "called");

    IPC::ResponseBuilder rb{ctx, 3};
    if (False(capability & Capability::StereoVision)) {
        LOG_ERROR(Service_PCTL, "Application does not have StereoVision capability!");
        rb.Push(PCTL::ResultNoCapability);
        rb.Push(false);
        return;
    }

    rb.Push(ResultSuccess);
    rb.Push(settings.is_stero_vision_restricted);
}

void IParentalControlService::ResetConfirmedStereoVisionPermission(HLERequestContext& ctx) {
    LOG_DEBUG(Service_PCTL, "called");

    states.stereo_vision = false;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

} // namespace Service::PCTL
