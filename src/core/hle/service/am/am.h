// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <chrono>
#include <memory>
#include <queue>

#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/service.h"

namespace Kernel {
class KernelCore;
class KReadableEvent;
class KTransferMemory;
} // namespace Kernel

namespace Service::Nvnflinger {
class Nvnflinger;
}

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

class IWindowController final : public ServiceFramework<IWindowController> {
public:
    explicit IWindowController(Core::System& system_);
    ~IWindowController() override;

private:
    void GetAppletResourceUserId(HLERequestContext& ctx);
    void AcquireForegroundRights(HLERequestContext& ctx);
};

class IAudioController final : public ServiceFramework<IAudioController> {
public:
    explicit IAudioController(Core::System& system_);
    ~IAudioController() override;

private:
    void SetExpectedMasterVolume(HLERequestContext& ctx);
    void GetMainAppletExpectedMasterVolume(HLERequestContext& ctx);
    void GetLibraryAppletExpectedMasterVolume(HLERequestContext& ctx);
    void ChangeMainAppletMasterVolume(HLERequestContext& ctx);
    void SetTransparentAudioRate(HLERequestContext& ctx);

    static constexpr float min_allowed_volume = 0.0f;
    static constexpr float max_allowed_volume = 1.0f;

    float main_applet_volume{0.25f};
    float library_applet_volume{max_allowed_volume};
    float transparent_volume_rate{min_allowed_volume};

    // Volume transition fade time in nanoseconds.
    // e.g. If the main applet volume was 0% and was changed to 50%
    //      with a fade of 50ns, then over the course of 50ns,
    //      the volume will gradually fade up to 50%
    std::chrono::nanoseconds fade_time_ns{0};
};

class IDisplayController final : public ServiceFramework<IDisplayController> {
public:
    explicit IDisplayController(Core::System& system_);
    ~IDisplayController() override;

private:
    void GetCallerAppletCaptureImageEx(HLERequestContext& ctx);
    void TakeScreenShotOfOwnLayer(HLERequestContext& ctx);
    void AcquireLastForegroundCaptureSharedBuffer(HLERequestContext& ctx);
    void ReleaseLastForegroundCaptureSharedBuffer(HLERequestContext& ctx);
    void AcquireCallerAppletCaptureSharedBuffer(HLERequestContext& ctx);
    void ReleaseCallerAppletCaptureSharedBuffer(HLERequestContext& ctx);
};

class IDebugFunctions final : public ServiceFramework<IDebugFunctions> {
public:
    explicit IDebugFunctions(Core::System& system_);
    ~IDebugFunctions() override;
};

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

    Result EnsureBufferSharingEnabled();

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

class ILockAccessor final : public ServiceFramework<ILockAccessor> {
public:
    explicit ILockAccessor(Core::System& system_);
    ~ILockAccessor() override;

private:
    void TryLock(HLERequestContext& ctx);
    void Unlock(HLERequestContext& ctx);
    void GetEvent(HLERequestContext& ctx);
    void IsLocked(HLERequestContext& ctx);

    bool is_locked{};

    Kernel::KEvent* lock_event;
    KernelHelpers::ServiceContext service_context;
};

class ICommonStateGetter final : public ServiceFramework<ICommonStateGetter> {
public:
    explicit ICommonStateGetter(Core::System& system_,
                                std::shared_ptr<AppletMessageQueue> msg_queue_);
    ~ICommonStateGetter() override;

private:
    // This is nn::oe::FocusState
    enum class FocusState : u8 {
        InFocus = 1,
        NotInFocus = 2,
        Background = 3,
    };

    // This is nn::oe::OperationMode
    enum class OperationMode : u8 {
        Handheld = 0,
        Docked = 1,
    };

    // This is nn::am::service::SystemButtonType
    enum class SystemButtonType {
        None,
        HomeButtonShortPressing,
        HomeButtonLongPressing,
        PowerButtonShortPressing,
        PowerButtonLongPressing,
        ShutdownSystem,
        CaptureButtonShortPressing,
        CaptureButtonLongPressing,
    };

    enum class SysPlatformRegion : s32 {
        Global = 1,
        Terra = 2,
    };

    void GetEventHandle(HLERequestContext& ctx);
    void ReceiveMessage(HLERequestContext& ctx);
    void GetCurrentFocusState(HLERequestContext& ctx);
    void RequestToAcquireSleepLock(HLERequestContext& ctx);
    void GetAcquiredSleepLockEvent(HLERequestContext& ctx);
    void GetReaderLockAccessorEx(HLERequestContext& ctx);
    void GetDefaultDisplayResolutionChangeEvent(HLERequestContext& ctx);
    void GetOperationMode(HLERequestContext& ctx);
    void GetPerformanceMode(HLERequestContext& ctx);
    void GetBootMode(HLERequestContext& ctx);
    void IsVrModeEnabled(HLERequestContext& ctx);
    void SetVrModeEnabled(HLERequestContext& ctx);
    void SetLcdBacklighOffEnabled(HLERequestContext& ctx);
    void BeginVrModeEx(HLERequestContext& ctx);
    void EndVrModeEx(HLERequestContext& ctx);
    void GetDefaultDisplayResolution(HLERequestContext& ctx);
    void SetCpuBoostMode(HLERequestContext& ctx);
    void GetBuiltInDisplayType(HLERequestContext& ctx);
    void PerformSystemButtonPressingIfInFocus(HLERequestContext& ctx);
    void GetSettingsPlatformRegion(HLERequestContext& ctx);
    void SetRequestExitToLibraryAppletAtExecuteNextProgramEnabled(HLERequestContext& ctx);

    std::shared_ptr<AppletMessageQueue> msg_queue;
    bool vr_mode_state{};
    Kernel::KEvent* sleep_lock_event;
    KernelHelpers::ServiceContext service_context;
};

class IStorageImpl {
public:
    virtual ~IStorageImpl();
    virtual std::vector<u8>& GetData() = 0;
    virtual const std::vector<u8>& GetData() const = 0;
    virtual std::size_t GetSize() const = 0;
};

class IStorage final : public ServiceFramework<IStorage> {
public:
    explicit IStorage(Core::System& system_, std::vector<u8>&& buffer);
    ~IStorage() override;

    std::vector<u8>& GetData() {
        return impl->GetData();
    }

    const std::vector<u8>& GetData() const {
        return impl->GetData();
    }

    std::size_t GetSize() const {
        return impl->GetSize();
    }

private:
    void Register();
    void Open(HLERequestContext& ctx);

    std::shared_ptr<IStorageImpl> impl;
};

class IStorageAccessor final : public ServiceFramework<IStorageAccessor> {
public:
    explicit IStorageAccessor(Core::System& system_, IStorage& backing_);
    ~IStorageAccessor() override;

private:
    void GetSize(HLERequestContext& ctx);
    void Write(HLERequestContext& ctx);
    void Read(HLERequestContext& ctx);

    IStorage& backing;
};

class ILibraryAppletCreator final : public ServiceFramework<ILibraryAppletCreator> {
public:
    explicit ILibraryAppletCreator(Core::System& system_);
    ~ILibraryAppletCreator() override;

private:
    void CreateLibraryApplet(HLERequestContext& ctx);
    void CreateStorage(HLERequestContext& ctx);
    void CreateTransferMemoryStorage(HLERequestContext& ctx);
    void CreateHandleStorage(HLERequestContext& ctx);
};

class ILibraryAppletSelfAccessor final : public ServiceFramework<ILibraryAppletSelfAccessor> {
public:
    explicit ILibraryAppletSelfAccessor(Core::System& system_);
    ~ILibraryAppletSelfAccessor() override;

private:
    void PopInData(HLERequestContext& ctx);
    void PushOutData(HLERequestContext& ctx);
    void GetLibraryAppletInfo(HLERequestContext& ctx);
    void ExitProcessAndReturn(HLERequestContext& ctx);
    void GetCallerAppletIdentityInfo(HLERequestContext& ctx);
    void GetMainAppletAvailableUsers(HLERequestContext& ctx);

    void PushInShowAlbum();
    void PushInShowCabinetData();
    void PushInShowMiiEditData();

    std::deque<std::vector<u8>> queue_data;
};

class IAppletCommonFunctions final : public ServiceFramework<IAppletCommonFunctions> {
public:
    explicit IAppletCommonFunctions(Core::System& system_);
    ~IAppletCommonFunctions() override;

private:
    void SetCpuBoostRequestPriority(HLERequestContext& ctx);
};

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

class IHomeMenuFunctions final : public ServiceFramework<IHomeMenuFunctions> {
public:
    explicit IHomeMenuFunctions(Core::System& system_);
    ~IHomeMenuFunctions() override;

private:
    void RequestToGetForeground(HLERequestContext& ctx);
    void GetPopFromGeneralChannelEvent(HLERequestContext& ctx);

    KernelHelpers::ServiceContext service_context;

    Kernel::KEvent* pop_from_general_channel_event;
};

class IGlobalStateController final : public ServiceFramework<IGlobalStateController> {
public:
    explicit IGlobalStateController(Core::System& system_);
    ~IGlobalStateController() override;
};

class IApplicationCreator final : public ServiceFramework<IApplicationCreator> {
public:
    explicit IApplicationCreator(Core::System& system_);
    ~IApplicationCreator() override;
};

class IProcessWindingController final : public ServiceFramework<IProcessWindingController> {
public:
    explicit IProcessWindingController(Core::System& system_);
    ~IProcessWindingController() override;

private:
    void GetLaunchReason(HLERequestContext& ctx);
    void OpenCallingLibraryApplet(HLERequestContext& ctx);
};

void LoopProcess(Nvnflinger::Nvnflinger& nvnflinger, Core::System& system);

} // namespace Service::AM
