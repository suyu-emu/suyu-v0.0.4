// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <queue>

#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/service.h"

namespace Kernel {
class KReadableEvent;
} // namespace Kernel

namespace Service::AM {

class AppletMessageQueue {
public:
    // This is nn::am::AppletMessage
    enum class AppletMessage : u32 {
        None = 0,
        ChangeIntoForeground = 1,
        ChangeIntoBackground = 2,
        Exit = 4,
        ApplicationExited = 6,
        FocusStateChanged = 15,
        Resume = 16,
        DetectShortPressingHomeButton = 20,
        DetectLongPressingHomeButton = 21,
        DetectShortPressingPowerButton = 22,
        DetectMiddlePressingPowerButton = 23,
        DetectLongPressingPowerButton = 24,
        RequestToPrepareSleep = 25,
        FinishedSleepSequence = 26,
        SleepRequiredByHighTemperature = 27,
        SleepRequiredByLowBattery = 28,
        AutoPowerDown = 29,
        OperationModeChanged = 30,
        PerformanceModeChanged = 31,
        DetectReceivingCecSystemStandby = 32,
        SdCardRemoved = 33,
        LaunchApplicationRequested = 50,
        RequestToDisplay = 51,
        ShowApplicationLogo = 55,
        HideApplicationLogo = 56,
        ForceHideApplicationLogo = 57,
        FloatingApplicationDetected = 60,
        DetectShortPressingCaptureButton = 90,
        AlbumScreenShotTaken = 92,
        AlbumRecordingSaved = 93,
    };

    explicit AppletMessageQueue(Core::System& system);
    ~AppletMessageQueue();

    Kernel::KReadableEvent& GetMessageReceiveEvent();
    Kernel::KReadableEvent& GetOperationModeChangedEvent();
    void PushMessage(AppletMessage msg);
    AppletMessage PopMessage();
    std::size_t GetMessageCount() const;
    void RequestExit();
    void RequestResume();
    void FocusStateChanged();
    void OperationModeChanged();

private:
    KernelHelpers::ServiceContext service_context;

    Kernel::KEvent* on_new_message;
    Kernel::KEvent* on_operation_mode_changed;

    std::queue<AppletMessage> messages;
};

} // namespace Service::AM
