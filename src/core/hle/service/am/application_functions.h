// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/service.h"

namespace Service::AM {

class IApplicationFunctions final : public ServiceFramework<IApplicationFunctions> {
public:
    explicit IApplicationFunctions(Core::System& system_);
    ~IApplicationFunctions() override;

private:
    void PopLaunchParameter(HLERequestContext& ctx);
    void CreateApplicationAndRequestToStartForQuest(HLERequestContext& ctx);
    void EnsureSaveData(HLERequestContext& ctx);
    void SetTerminateResult(HLERequestContext& ctx);
    void GetDisplayVersion(HLERequestContext& ctx);
    void GetDesiredLanguage(HLERequestContext& ctx);
    void IsGamePlayRecordingSupported(HLERequestContext& ctx);
    void InitializeGamePlayRecording(HLERequestContext& ctx);
    void SetGamePlayRecordingState(HLERequestContext& ctx);
    void NotifyRunning(HLERequestContext& ctx);
    void GetPseudoDeviceId(HLERequestContext& ctx);
    void ExtendSaveData(HLERequestContext& ctx);
    void GetSaveDataSize(HLERequestContext& ctx);
    void CreateCacheStorage(HLERequestContext& ctx);
    void GetSaveDataSizeMax(HLERequestContext& ctx);
    void BeginBlockingHomeButtonShortAndLongPressed(HLERequestContext& ctx);
    void EndBlockingHomeButtonShortAndLongPressed(HLERequestContext& ctx);
    void BeginBlockingHomeButton(HLERequestContext& ctx);
    void EndBlockingHomeButton(HLERequestContext& ctx);
    void EnableApplicationCrashReport(HLERequestContext& ctx);
    void InitializeApplicationCopyrightFrameBuffer(HLERequestContext& ctx);
    void SetApplicationCopyrightImage(HLERequestContext& ctx);
    void SetApplicationCopyrightVisibility(HLERequestContext& ctx);
    void QueryApplicationPlayStatistics(HLERequestContext& ctx);
    void QueryApplicationPlayStatisticsByUid(HLERequestContext& ctx);
    void ExecuteProgram(HLERequestContext& ctx);
    void ClearUserChannel(HLERequestContext& ctx);
    void UnpopToUserChannel(HLERequestContext& ctx);
    void GetPreviousProgramIndex(HLERequestContext& ctx);
    void GetGpuErrorDetectedSystemEvent(HLERequestContext& ctx);
    void GetFriendInvitationStorageChannelEvent(HLERequestContext& ctx);
    void TryPopFromFriendInvitationStorageChannel(HLERequestContext& ctx);
    void GetNotificationStorageChannelEvent(HLERequestContext& ctx);
    void GetHealthWarningDisappearedSystemEvent(HLERequestContext& ctx);
    void PrepareForJit(HLERequestContext& ctx);

    KernelHelpers::ServiceContext service_context;

    bool launch_popped_account_preselect = false;
    s32 previous_program_index{-1};
    Kernel::KEvent* gpu_error_detected_event;
    Kernel::KEvent* friend_invitation_storage_channel_event;
    Kernel::KEvent* notification_storage_channel_event;
    Kernel::KEvent* health_warning_disappeared_system_event;
};

} // namespace Service::AM
