// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/am/applets/applets.h"
#include "core/hle/service/am/self_controller.h"
#include "core/hle/service/caps/caps_su.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/nvnflinger/fb_share_buffer_manager.h"
#include "core/hle/service/nvnflinger/nvnflinger.h"
#include "core/hle/service/sm/sm.h"
#include "core/hle/service/vi/vi_results.h"

namespace Service::AM {

ISelfController::ISelfController(Core::System& system_, Nvnflinger::Nvnflinger& nvnflinger_)
    : ServiceFramework{system_, "ISelfController"}, nvnflinger{nvnflinger_},
      service_context{system, "ISelfController"} {
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
        {15, nullptr, "SetScreenShotAppletIdentityInfo"},
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
        {61, nullptr, "SetMediaPlaybackState"},
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

    launchable_event = service_context.CreateEvent("ISelfController:LaunchableEvent");

    // This event is created by AM on the first time GetAccumulatedSuspendedTickChangedEvent() is
    // called. Yuzu can just create it unconditionally, since it doesn't need to support multiple
    // ISelfControllers. The event is signaled on creation, and on transition from suspended -> not
    // suspended if the event has previously been created by a call to
    // GetAccumulatedSuspendedTickChangedEvent.

    accumulated_suspended_tick_changed_event =
        service_context.CreateEvent("ISelfController:AccumulatedSuspendedTickChangedEvent");
    accumulated_suspended_tick_changed_event->Signal();
}

ISelfController::~ISelfController() {
    service_context.CloseEvent(launchable_event);
    service_context.CloseEvent(accumulated_suspended_tick_changed_event);
}

void ISelfController::Exit(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);

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
    ++num_fatal_sections_entered;
    LOG_DEBUG(Service_AM, "called. Num fatal sections entered: {}", num_fatal_sections_entered);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISelfController::LeaveFatalSection(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called.");

    // Entry and exit of fatal sections must be balanced.
    if (num_fatal_sections_entered == 0) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(Result{ErrorModule::AM, 512});
        return;
    }

    --num_fatal_sections_entered;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISelfController::GetLibraryAppletLaunchableEvent(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    launchable_event->Signal();

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(launchable_event->GetReadableEvent());
}

void ISelfController::SetScreenShotPermission(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto permission = rp.PopEnum<ScreenshotPermission>();
    LOG_DEBUG(Service_AM, "called, permission={}", permission);

    screenshot_permission = permission;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISelfController::SetOperationModeChangedNotification(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    bool flag = rp.Pop<bool>();
    LOG_WARNING(Service_AM, "(STUBBED) called flag={}", flag);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISelfController::SetPerformanceModeChangedNotification(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    bool flag = rp.Pop<bool>();
    LOG_WARNING(Service_AM, "(STUBBED) called flag={}", flag);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISelfController::SetFocusHandlingMode(HLERequestContext& ctx) {
    // Takes 3 input u8s with each field located immediately after the previous
    // u8, these are bool flags. No output.
    IPC::RequestParser rp{ctx};

    struct FocusHandlingModeParams {
        u8 unknown0;
        u8 unknown1;
        u8 unknown2;
    };
    const auto flags = rp.PopRaw<FocusHandlingModeParams>();

    LOG_WARNING(Service_AM, "(STUBBED) called. unknown0={}, unknown1={}, unknown2={}",
                flags.unknown0, flags.unknown1, flags.unknown2);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISelfController::SetRestartMessageEnabled(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISelfController::SetOutOfFocusSuspendingEnabled(HLERequestContext& ctx) {
    // Takes 3 input u8s with each field located immediately after the previous
    // u8, these are bool flags. No output.
    IPC::RequestParser rp{ctx};

    bool enabled = rp.Pop<bool>();
    LOG_WARNING(Service_AM, "(STUBBED) called enabled={}", enabled);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISelfController::SetAlbumImageOrientation(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISelfController::CreateManagedDisplayLayer(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    // TODO(Subv): Find out how AM determines the display to use, for now just
    // create the layer in the Default display.
    const auto display_id = nvnflinger.OpenDisplay("Default");
    const auto layer_id = nvnflinger.CreateLayer(*display_id);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push(*layer_id);
}

void ISelfController::IsSystemBufferSharingEnabled(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(this->EnsureBufferSharingEnabled(ctx.GetThread().GetOwnerProcess()));
}

void ISelfController::GetSystemSharedLayerHandle(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(this->EnsureBufferSharingEnabled(ctx.GetThread().GetOwnerProcess()));
    rb.Push<s64>(system_shared_buffer_id);
    rb.Push<s64>(system_shared_layer_id);
}

void ISelfController::GetSystemSharedBufferHandle(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(this->EnsureBufferSharingEnabled(ctx.GetThread().GetOwnerProcess()));
    rb.Push<s64>(system_shared_buffer_id);
}

Result ISelfController::EnsureBufferSharingEnabled(Kernel::KProcess* process) {
    if (buffer_sharing_enabled) {
        return ResultSuccess;
    }

    if (system.GetAppletManager().GetCurrentAppletId() <= Applets::AppletId::Application) {
        return VI::ResultOperationFailed;
    }

    const auto display_id = nvnflinger.OpenDisplay("Default");
    const auto result = nvnflinger.GetSystemBufferManager().Initialize(
        &system_shared_buffer_id, &system_shared_layer_id, *display_id);

    if (result.IsSuccess()) {
        buffer_sharing_enabled = true;
    }

    return result;
}

void ISelfController::CreateManagedDisplaySeparableLayer(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    // TODO(Subv): Find out how AM determines the display to use, for now just
    // create the layer in the Default display.
    // This calls nn::vi::CreateRecordingLayer() which creates another layer.
    // Currently we do not support more than 1 layer per display, output 1 layer id for now.
    // Outputting 1 layer id instead of the expected 2 has not been observed to cause any adverse
    // side effects.
    // TODO: Support multiple layers
    const auto display_id = nvnflinger.OpenDisplay("Default");
    const auto layer_id = nvnflinger.CreateLayer(*display_id);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push(*layer_id);
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

void ISelfController::SetIdleTimeDetectionExtension(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    idle_time_detection_extension = rp.Pop<u32>();
    LOG_DEBUG(Service_AM, "(STUBBED) called idle_time_detection_extension={}",
              idle_time_detection_extension);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISelfController::GetIdleTimeDetectionExtension(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u32>(idle_time_detection_extension);
}

void ISelfController::ReportUserIsActive(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISelfController::SetAutoSleepDisabled(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    is_auto_sleep_disabled = rp.Pop<bool>();

    // On the system itself, if the previous state of is_auto_sleep_disabled
    // differed from the current value passed in, it'd signify the internal
    // window manager to update (and also increment some statistics like update counts)
    //
    // It'd also indicate this change to an idle handling context.
    //
    // However, given we're emulating this behavior, most of this can be ignored
    // and it's sufficient to simply set the member variable for querying via
    // IsAutoSleepDisabled().

    LOG_DEBUG(Service_AM, "called. is_auto_sleep_disabled={}", is_auto_sleep_disabled);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISelfController::IsAutoSleepDisabled(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called.");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(is_auto_sleep_disabled);
}

void ISelfController::GetAccumulatedSuspendedTickValue(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called.");

    // This command returns the total number of system ticks since ISelfController creation
    // where the game was suspended. Since Yuzu doesn't implement game suspension, this command
    // can just always return 0 ticks.
    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push<u64>(0);
}

void ISelfController::GetAccumulatedSuspendedTickChangedEvent(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called.");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(accumulated_suspended_tick_changed_event->GetReadableEvent());
}

void ISelfController::SetAlbumImageTakenNotificationEnabled(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    // This service call sets an internal flag whether a notification is shown when an image is
    // captured. Currently we do not support capturing images via the capture button, so this can be
    // stubbed for now.
    const bool album_image_taken_notification_enabled = rp.Pop<bool>();

    LOG_WARNING(Service_AM, "(STUBBED) called. album_image_taken_notification_enabled={}",
                album_image_taken_notification_enabled);

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

    const auto is_record_volume_muted = rp.Pop<bool>();

    LOG_WARNING(Service_AM, "(STUBBED) called. is_record_volume_muted={}", is_record_volume_muted);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

} // namespace Service::AM
