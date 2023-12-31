// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/service.h"

namespace Service::AM {

class ISelfController final : public ServiceFramework<ISelfController> {
public:
    explicit ISelfController(Core::System& system_, Nvnflinger::Nvnflinger& nvnflinger_);
    ~ISelfController() override;

private:
    void Exit(HLERequestContext& ctx);
    void LockExit(HLERequestContext& ctx);
    void UnlockExit(HLERequestContext& ctx);
    void EnterFatalSection(HLERequestContext& ctx);
    void LeaveFatalSection(HLERequestContext& ctx);
    void GetLibraryAppletLaunchableEvent(HLERequestContext& ctx);
    void SetScreenShotPermission(HLERequestContext& ctx);
    void SetOperationModeChangedNotification(HLERequestContext& ctx);
    void SetPerformanceModeChangedNotification(HLERequestContext& ctx);
    void SetFocusHandlingMode(HLERequestContext& ctx);
    void SetRestartMessageEnabled(HLERequestContext& ctx);
    void SetOutOfFocusSuspendingEnabled(HLERequestContext& ctx);
    void SetAlbumImageOrientation(HLERequestContext& ctx);
    void IsSystemBufferSharingEnabled(HLERequestContext& ctx);
    void GetSystemSharedBufferHandle(HLERequestContext& ctx);
    void GetSystemSharedLayerHandle(HLERequestContext& ctx);
    void CreateManagedDisplayLayer(HLERequestContext& ctx);
    void CreateManagedDisplaySeparableLayer(HLERequestContext& ctx);
    void SetHandlesRequestToDisplay(HLERequestContext& ctx);
    void ApproveToDisplay(HLERequestContext& ctx);
    void SetIdleTimeDetectionExtension(HLERequestContext& ctx);
    void GetIdleTimeDetectionExtension(HLERequestContext& ctx);
    void ReportUserIsActive(HLERequestContext& ctx);
    void SetAutoSleepDisabled(HLERequestContext& ctx);
    void IsAutoSleepDisabled(HLERequestContext& ctx);
    void GetAccumulatedSuspendedTickValue(HLERequestContext& ctx);
    void GetAccumulatedSuspendedTickChangedEvent(HLERequestContext& ctx);
    void SetAlbumImageTakenNotificationEnabled(HLERequestContext& ctx);
    void SaveCurrentScreenshot(HLERequestContext& ctx);
    void SetRecordVolumeMuted(HLERequestContext& ctx);

    Result EnsureBufferSharingEnabled(Kernel::KProcess* process);

    enum class ScreenshotPermission : u32 {
        Inherit = 0,
        Enable = 1,
        Disable = 2,
    };

    Nvnflinger::Nvnflinger& nvnflinger;

    KernelHelpers::ServiceContext service_context;

    Kernel::KEvent* launchable_event;
    Kernel::KEvent* accumulated_suspended_tick_changed_event;

    u32 idle_time_detection_extension = 0;
    u64 num_fatal_sections_entered = 0;
    u64 system_shared_buffer_id = 0;
    u64 system_shared_layer_id = 0;
    bool is_auto_sleep_disabled = false;
    bool buffer_sharing_enabled = false;
    ScreenshotPermission screenshot_permission = ScreenshotPermission::Inherit;
};

} // namespace Service::AM
