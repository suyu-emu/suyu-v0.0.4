// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging/log.h"
#include "core/hle/result.h"
#include "core/hle/service/am/am_results.h"
#include "core/hle/service/am/frontend/applets.h"
#include "core/hle/service/am/self_controller.h"
#include "core/hle/service/caps/caps_su.h"
#include "core/hle/service/hle_ipc.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/nvnflinger/fb_share_buffer_manager.h"
#include "core/hle/service/nvnflinger/nvnflinger.h"
#include "core/hle/service/sm/sm.h"
#include "core/hle/service/vi/vi_results.h"

namespace Service::AM {

ISelfController::ISelfController(Core::System& system_, std::shared_ptr<Applet> applet_,
                                 Nvnflinger::Nvnflinger& nvnflinger_)
    : ServiceFramework{system_, "ISelfController"}, nvnflinger{nvnflinger_}, applet{std::move(
                                                                                 applet_)} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &ISelfController::Exit, "Exit"},
        {1, &ISelfController::LockExit, "LockExit"},
        {2, &ISelfController::UnlockExit, "UnlockExit"},
        {3, &ISelfController::EnterFatalSection, "EnterFatalSection"},
        {4, &ISelfController::LeaveFatalSection, "LeaveFatalSection"},
        {9, &ISelfController::GetLibraryAppletLaunchableEvent, "GetLibraryAppletLaunchableEvent"},
        {10, &ISelfController::SetScreenShotPermission, "SetScreenShotPermission"},
        {11, &ISelfController::SetOperationModeChangedNotification, "SetOperationModeChangedNotification"},
        {12, &ISelfController::SetPerformanceModeChangedNotification, "SetPerformanceModeChangedNotification"},
        {13, &ISelfController::SetFocusHandlingMode, "SetFocusHandlingMode"},
        {14, &ISelfController::SetRestartMessageEnabled, "SetRestartMessageEnabled"},
        {15, &ISelfController::SetScreenShotAppletIdentityInfo, "SetScreenShotAppletIdentityInfo"},
        {16, &ISelfController::SetOutOfFocusSuspendingEnabled, "SetOutOfFocusSuspendingEnabled"},
        {17, nullptr, "SetControllerFirmwareUpdateSection"},
        {18, nullptr, "SetRequiresCaptureButtonShortPressedMessage"},
        {19, &ISelfController::SetAlbumImageOrientation, "SetAlbumImageOrientation"},
        {20, nullptr, "SetDesirableKeyboardLayout"},
        {21, nullptr, "GetScreenShotProgramId"},
        {40, &ISelfController::CreateManagedDisplayLayer, "CreateManagedDisplayLayer"},
        {41, &ISelfController::IsSystemBufferSharingEnabled, "IsSystemBufferSharingEnabled"},
        {42, &ISelfController::GetSystemSharedLayerHandle, "GetSystemSharedLayerHandle"},
        {43, &ISelfController::GetSystemSharedBufferHandle, "GetSystemSharedBufferHandle"},
        {44, &ISelfController::CreateManagedDisplaySeparableLayer, "CreateManagedDisplaySeparableLayer"},
        {45, nullptr, "SetManagedDisplayLayerSeparationMode"},
        {46, nullptr, "SetRecordingLayerCompositionEnabled"},
        {50, &ISelfController::SetHandlesRequestToDisplay, "SetHandlesRequestToDisplay"},
        {51, &ISelfController::ApproveToDisplay, "ApproveToDisplay"},
        {60, nullptr, "OverrideAutoSleepTimeAndDimmingTime"},
        {61, &ISelfController::SetMediaPlaybackState, "SetMediaPlaybackState"},
        {62, &ISelfController::SetIdleTimeDetectionExtension, "SetIdleTimeDetectionExtension"},
        {63, &ISelfController::GetIdleTimeDetectionExtension, "GetIdleTimeDetectionExtension"},
        {64, nullptr, "SetInputDetectionSourceSet"},
        {65, &ISelfController::ReportUserIsActive, "ReportUserIsActive"},
        {66, nullptr, "GetCurrentIlluminance"},
        {67, nullptr, "IsIlluminanceAvailable"},
        {68, &ISelfController::SetAutoSleepDisabled, "SetAutoSleepDisabled"},
        {69, &ISelfController::IsAutoSleepDisabled, "IsAutoSleepDisabled"},
        {70, nullptr, "ReportMultimediaError"},
        {71, nullptr, "GetCurrentIlluminanceEx"},
        {72, nullptr, "SetInputDetectionPolicy"},
        {80, nullptr, "SetWirelessPriorityMode"},
        {90, &ISelfController::GetAccumulatedSuspendedTickValue, "GetAccumulatedSuspendedTickValue"},
        {91, &ISelfController::GetAccumulatedSuspendedTickChangedEvent, "GetAccumulatedSuspendedTickChangedEvent"},
        {100, &ISelfController::SetAlbumImageTakenNotificationEnabled, "SetAlbumImageTakenNotificationEnabled"},
        {110, nullptr, "SetApplicationAlbumUserData"},
        {120, &ISelfController::SaveCurrentScreenshot, "SaveCurrentScreenshot"},
        {130, &ISelfController::SetRecordVolumeMuted, "SetRecordVolumeMuted"},
        {1000, nullptr, "GetDebugStorageChannel"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

ISelfController::~ISelfController() = default;

void ISelfController::Exit(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);

    // TODO
    system.Exit();
}

void ISelfController::LockExit(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    system.SetExitLocked(true);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISelfController::UnlockExit(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    system.SetExitLocked(false);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);

    if (system.GetExitRequested()) {
        system.Exit();
    }
}

void ISelfController::EnterFatalSection(HLERequestContext& ctx) {

    std::scoped_lock lk{applet->lock};
    applet->fatal_section_count++;
    LOG_DEBUG(Service_AM, "called. Num fatal sections entered: {}", applet->fatal_section_count);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISelfController::LeaveFatalSection(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called.");

    // Entry and exit of fatal sections must be balanced.
    std::scoped_lock lk{applet->lock};
    if (applet->fatal_section_count == 0) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(AM::ResultFatalSectionCountImbalance);
        return;
    }

    applet->fatal_section_count--;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISelfController::GetLibraryAppletLaunchableEvent(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    applet->library_applet_launchable_event.Signal();

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(applet->library_applet_launchable_event.GetHandle());
}

void ISelfController::SetScreenShotPermission(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto permission = rp.PopEnum<ScreenshotPermission>();
    LOG_DEBUG(Service_AM, "called, permission={}", permission);

    std::scoped_lock lk{applet->lock};
    applet->screenshot_permission = permission;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISelfController::SetOperationModeChangedNotification(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    const bool notification_enabled = rp.Pop<bool>();
    LOG_WARNING(Service_AM, "(STUBBED) called notification_enabled={}", notification_enabled);

    std::scoped_lock lk{applet->lock};
    applet->operation_mode_changed_notification_enabled = notification_enabled;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISelfController::SetPerformanceModeChangedNotification(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    const bool notification_enabled = rp.Pop<bool>();
    LOG_WARNING(Service_AM, "(STUBBED) called notification_enabled={}", notification_enabled);

    std::scoped_lock lk{applet->lock};
    applet->performance_mode_changed_notification_enabled = notification_enabled;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISelfController::SetFocusHandlingMode(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    const auto flags = rp.PopRaw<FocusHandlingMode>();

    LOG_WARNING(Service_AM, "(STUBBED) called. unknown0={}, unknown1={}, unknown2={}",
                flags.unknown0, flags.unknown1, flags.unknown2);

    std::scoped_lock lk{applet->lock};
    applet->focus_handling_mode = flags;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISelfController::SetRestartMessageEnabled(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    std::scoped_lock lk{applet->lock};
    applet->restart_message_enabled = true;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISelfController::SetScreenShotAppletIdentityInfo(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::RequestParser rp{ctx};
    std::scoped_lock lk{applet->lock};
    applet->screen_shot_identity = rp.PopRaw<AppletIdentityInfo>();

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISelfController::SetOutOfFocusSuspendingEnabled(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    const bool enabled = rp.Pop<bool>();
    LOG_WARNING(Service_AM, "(STUBBED) called enabled={}", enabled);

    std::scoped_lock lk{applet->lock};
    ASSERT(applet->type == AppletType::Application);
    applet->out_of_focus_suspension_enabled = enabled;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISelfController::SetAlbumImageOrientation(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    const auto orientation = rp.PopRaw<Capture::AlbumImageOrientation>();
    LOG_WARNING(Service_AM, "(STUBBED) called, orientation={}", static_cast<s32>(orientation));

    std::scoped_lock lk{applet->lock};
    applet->album_image_orientation = orientation;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISelfController::CreateManagedDisplayLayer(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    u64 layer_id{};
    applet->managed_layer_holder.Initialize(&nvnflinger);
    applet->managed_layer_holder.CreateManagedDisplayLayer(&layer_id);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push(layer_id);
}

void ISelfController::IsSystemBufferSharingEnabled(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(this->EnsureBufferSharingEnabled(ctx.GetThread().GetOwnerProcess()));
}

void ISelfController::GetSystemSharedLayerHandle(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    u64 buffer_id, layer_id;
    applet->system_buffer_manager.GetSystemSharedLayerHandle(&buffer_id, &layer_id);

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(this->EnsureBufferSharingEnabled(ctx.GetThread().GetOwnerProcess()));
    rb.Push<s64>(buffer_id);
    rb.Push<s64>(layer_id);
}

void ISelfController::GetSystemSharedBufferHandle(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    u64 buffer_id, layer_id;
    applet->system_buffer_manager.GetSystemSharedLayerHandle(&buffer_id, &layer_id);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(this->EnsureBufferSharingEnabled(ctx.GetThread().GetOwnerProcess()));
    rb.Push<s64>(buffer_id);
}

Result ISelfController::EnsureBufferSharingEnabled(Kernel::KProcess* process) {
    if (applet->system_buffer_manager.Initialize(&nvnflinger, process, applet->applet_id,
                                                 applet->library_applet_mode)) {
        return ResultSuccess;
    }

    return VI::ResultOperationFailed;
}

void ISelfController::CreateManagedDisplaySeparableLayer(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    u64 layer_id{};
    u64 recording_layer_id{};
    applet->managed_layer_holder.Initialize(&nvnflinger);
    applet->managed_layer_holder.CreateManagedDisplaySeparableLayer(&layer_id, &recording_layer_id);

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(ResultSuccess);
    rb.Push(layer_id);
    rb.Push(recording_layer_id);
}

void ISelfController::SetHandlesRequestToDisplay(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISelfController::ApproveToDisplay(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISelfController::SetMediaPlaybackState(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const u8 state = rp.Pop<u8>();

    LOG_WARNING(Service_AM, "(STUBBED) called, state={}", state);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISelfController::SetIdleTimeDetectionExtension(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    const auto extension = rp.PopRaw<IdleTimeDetectionExtension>();
    LOG_DEBUG(Service_AM, "(STUBBED) called extension={}", extension);

    std::scoped_lock lk{applet->lock};
    applet->idle_time_detection_extension = extension;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISelfController::GetIdleTimeDetectionExtension(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    std::scoped_lock lk{applet->lock};

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushRaw<IdleTimeDetectionExtension>(applet->idle_time_detection_extension);
}

void ISelfController::ReportUserIsActive(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISelfController::SetAutoSleepDisabled(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    std::scoped_lock lk{applet->lock};
    applet->auto_sleep_disabled = rp.Pop<bool>();

    // On the system itself, if the previous state of is_auto_sleep_disabled
    // differed from the current value passed in, it'd signify the internal
    // window manager to update (and also increment some statistics like update counts)
    //
    // It'd also indicate this change to an idle handling context.
    //
    // However, given we're emulating this behavior, most of this can be ignored
    // and it's sufficient to simply set the member variable for querying via
    // IsAutoSleepDisabled().

    LOG_DEBUG(Service_AM, "called. is_auto_sleep_disabled={}", applet->auto_sleep_disabled);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISelfController::IsAutoSleepDisabled(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called.");

    std::scoped_lock lk{applet->lock};

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(applet->auto_sleep_disabled);
}

void ISelfController::GetAccumulatedSuspendedTickValue(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called.");

    std::scoped_lock lk{applet->lock};
    // This command returns the total number of system ticks since ISelfController creation
    // where the game was suspended. Since Yuzu doesn't implement game suspension, this command
    // can just always return 0 ticks.
    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push<u64>(applet->suspended_ticks);
}

void ISelfController::GetAccumulatedSuspendedTickChangedEvent(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called.");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(applet->accumulated_suspended_tick_changed_event.GetHandle());
}

void ISelfController::SetAlbumImageTakenNotificationEnabled(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    // This service call sets an internal flag whether a notification is shown when an image is
    // captured. Currently we do not support capturing images via the capture button, so this can be
    // stubbed for now.
    const bool enabled = rp.Pop<bool>();
    LOG_WARNING(Service_AM, "(STUBBED) called. enabled={}", enabled);

    std::scoped_lock lk{applet->lock};
    applet->album_image_taken_notification_enabled = enabled;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISelfController::SaveCurrentScreenshot(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    const auto report_option = rp.PopEnum<Capture::AlbumReportOption>();

    LOG_INFO(Service_AM, "called, report_option={}", report_option);

    const auto screenshot_service =
        system.ServiceManager().GetService<Service::Capture::IScreenShotApplicationService>(
            "caps:su");

    if (screenshot_service) {
        screenshot_service->CaptureAndSaveScreenshot(report_option);
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISelfController::SetRecordVolumeMuted(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    const auto enabled = rp.Pop<bool>();
    LOG_WARNING(Service_AM, "(STUBBED) called. enabled={}", enabled);

    std::scoped_lock lk{applet->lock};
    applet->record_volume_muted = enabled;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

} // namespace Service::AM
