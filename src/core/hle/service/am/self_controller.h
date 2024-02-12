// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/hle_ipc.h"
#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/service.h"

namespace Service::AM {

struct Applet;

class ISelfController final : public ServiceFramework<ISelfController> {
public:
    explicit ISelfController(Core::System& system_, std::shared_ptr<Applet> applet_,
                             Nvnflinger::Nvnflinger& nvnflinger_);
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
    void SetScreenShotAppletIdentityInfo(HLERequestContext& ctx);
    void SetOutOfFocusSuspendingEnabled(HLERequestContext& ctx);
    void SetAlbumImageOrientation(HLERequestContext& ctx);
    void IsSystemBufferSharingEnabled(HLERequestContext& ctx);
    void GetSystemSharedBufferHandle(HLERequestContext& ctx);
    void GetSystemSharedLayerHandle(HLERequestContext& ctx);
    void CreateManagedDisplayLayer(HLERequestContext& ctx);
    void CreateManagedDisplaySeparableLayer(HLERequestContext& ctx);
    void SetHandlesRequestToDisplay(HLERequestContext& ctx);
    void ApproveToDisplay(HLERequestContext& ctx);
    void SetMediaPlaybackState(HLERequestContext& ctx);
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

    Nvnflinger::Nvnflinger& nvnflinger;
    const std::shared_ptr<Applet> applet;
};

} // namespace Service::AM
