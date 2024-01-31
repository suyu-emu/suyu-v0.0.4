// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/settings.h"
#include "common/uuid.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/savedata_factory.h"
#include "core/hle/service/acc/profile_manager.h"
#include "core/hle/service/am/am_results.h"
#include "core/hle/service/am/applet.h"
#include "core/hle/service/am/application_functions.h"
#include "core/hle/service/am/storage.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/hle/service/filesystem/save_data_controller.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/ns/ns.h"
#include "core/hle/service/sm/sm.h"

namespace Service::AM {

enum class LaunchParameterKind : u32 {
    UserChannel = 1,
    AccountPreselectedUser = 2,
};

IApplicationFunctions::IApplicationFunctions(Core::System& system_, std::shared_ptr<Applet> applet_)
    : ServiceFramework{system_, "IApplicationFunctions"}, applet{std::move(applet_)} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {1, &IApplicationFunctions::PopLaunchParameter, "PopLaunchParameter"},
        {10, nullptr, "CreateApplicationAndPushAndRequestToStart"},
        {11, nullptr, "CreateApplicationAndPushAndRequestToStartForQuest"},
        {12, nullptr, "CreateApplicationAndRequestToStart"},
        {13, &IApplicationFunctions::CreateApplicationAndRequestToStartForQuest, "CreateApplicationAndRequestToStartForQuest"},
        {14, nullptr, "CreateApplicationWithAttributeAndPushAndRequestToStartForQuest"},
        {15, nullptr, "CreateApplicationWithAttributeAndRequestToStartForQuest"},
        {20, &IApplicationFunctions::EnsureSaveData, "EnsureSaveData"},
        {21, &IApplicationFunctions::GetDesiredLanguage, "GetDesiredLanguage"},
        {22, &IApplicationFunctions::SetTerminateResult, "SetTerminateResult"},
        {23, &IApplicationFunctions::GetDisplayVersion, "GetDisplayVersion"},
        {24, nullptr, "GetLaunchStorageInfoForDebug"},
        {25, &IApplicationFunctions::ExtendSaveData, "ExtendSaveData"},
        {26, &IApplicationFunctions::GetSaveDataSize, "GetSaveDataSize"},
        {27, &IApplicationFunctions::CreateCacheStorage, "CreateCacheStorage"},
        {28, &IApplicationFunctions::GetSaveDataSizeMax, "GetSaveDataSizeMax"},
        {29, nullptr, "GetCacheStorageMax"},
        {30, &IApplicationFunctions::BeginBlockingHomeButtonShortAndLongPressed, "BeginBlockingHomeButtonShortAndLongPressed"},
        {31, &IApplicationFunctions::EndBlockingHomeButtonShortAndLongPressed, "EndBlockingHomeButtonShortAndLongPressed"},
        {32, &IApplicationFunctions::BeginBlockingHomeButton, "BeginBlockingHomeButton"},
        {33, &IApplicationFunctions::EndBlockingHomeButton, "EndBlockingHomeButton"},
        {34, nullptr, "SelectApplicationLicense"},
        {35, nullptr, "GetDeviceSaveDataSizeMax"},
        {36, nullptr, "GetLimitedApplicationLicense"},
        {37, nullptr, "GetLimitedApplicationLicenseUpgradableEvent"},
        {40, &IApplicationFunctions::NotifyRunning, "NotifyRunning"},
        {50, &IApplicationFunctions::GetPseudoDeviceId, "GetPseudoDeviceId"},
        {60, nullptr, "SetMediaPlaybackStateForApplication"},
        {65, &IApplicationFunctions::IsGamePlayRecordingSupported, "IsGamePlayRecordingSupported"},
        {66, &IApplicationFunctions::InitializeGamePlayRecording, "InitializeGamePlayRecording"},
        {67, &IApplicationFunctions::SetGamePlayRecordingState, "SetGamePlayRecordingState"},
        {68, nullptr, "RequestFlushGamePlayingMovieForDebug"},
        {70, nullptr, "RequestToShutdown"},
        {71, nullptr, "RequestToReboot"},
        {72, nullptr, "RequestToSleep"},
        {80, nullptr, "ExitAndRequestToShowThanksMessage"},
        {90, &IApplicationFunctions::EnableApplicationCrashReport, "EnableApplicationCrashReport"},
        {100, &IApplicationFunctions::InitializeApplicationCopyrightFrameBuffer, "InitializeApplicationCopyrightFrameBuffer"},
        {101, &IApplicationFunctions::SetApplicationCopyrightImage, "SetApplicationCopyrightImage"},
        {102, &IApplicationFunctions::SetApplicationCopyrightVisibility, "SetApplicationCopyrightVisibility"},
        {110, &IApplicationFunctions::QueryApplicationPlayStatistics, "QueryApplicationPlayStatistics"},
        {111, &IApplicationFunctions::QueryApplicationPlayStatisticsByUid, "QueryApplicationPlayStatisticsByUid"},
        {120, &IApplicationFunctions::ExecuteProgram, "ExecuteProgram"},
        {121, &IApplicationFunctions::ClearUserChannel, "ClearUserChannel"},
        {122, &IApplicationFunctions::UnpopToUserChannel, "UnpopToUserChannel"},
        {123, &IApplicationFunctions::GetPreviousProgramIndex, "GetPreviousProgramIndex"},
        {124, nullptr, "EnableApplicationAllThreadDumpOnCrash"},
        {130, &IApplicationFunctions::GetGpuErrorDetectedSystemEvent, "GetGpuErrorDetectedSystemEvent"},
        {131, nullptr, "SetDelayTimeToAbortOnGpuError"},
        {140, &IApplicationFunctions::GetFriendInvitationStorageChannelEvent, "GetFriendInvitationStorageChannelEvent"},
        {141, &IApplicationFunctions::TryPopFromFriendInvitationStorageChannel, "TryPopFromFriendInvitationStorageChannel"},
        {150, &IApplicationFunctions::GetNotificationStorageChannelEvent, "GetNotificationStorageChannelEvent"},
        {151, nullptr, "TryPopFromNotificationStorageChannel"},
        {160, &IApplicationFunctions::GetHealthWarningDisappearedSystemEvent, "GetHealthWarningDisappearedSystemEvent"},
        {170, nullptr, "SetHdcpAuthenticationActivated"},
        {180, nullptr, "GetLaunchRequiredVersion"},
        {181, nullptr, "UpgradeLaunchRequiredVersion"},
        {190, nullptr, "SendServerMaintenanceOverlayNotification"},
        {200, nullptr, "GetLastApplicationExitReason"},
        {500, nullptr, "StartContinuousRecordingFlushForDebug"},
        {1000, nullptr, "CreateMovieMaker"},
        {1001, &IApplicationFunctions::PrepareForJit, "PrepareForJit"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

IApplicationFunctions::~IApplicationFunctions() = default;

void IApplicationFunctions::EnableApplicationCrashReport(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    std::scoped_lock lk{applet->lock};
    applet->application_crash_report_enabled = true;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IApplicationFunctions::InitializeApplicationCopyrightFrameBuffer(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IApplicationFunctions::SetApplicationCopyrightImage(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IApplicationFunctions::SetApplicationCopyrightVisibility(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto is_visible = rp.Pop<bool>();

    LOG_WARNING(Service_AM, "(STUBBED) called, is_visible={}", is_visible);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IApplicationFunctions::BeginBlockingHomeButtonShortAndLongPressed(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    std::scoped_lock lk{applet->lock};
    applet->home_button_long_pressed_blocked = true;
    applet->home_button_short_pressed_blocked = true;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IApplicationFunctions::EndBlockingHomeButtonShortAndLongPressed(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    std::scoped_lock lk{applet->lock};
    applet->home_button_long_pressed_blocked = false;
    applet->home_button_short_pressed_blocked = false;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IApplicationFunctions::BeginBlockingHomeButton(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    std::scoped_lock lk{applet->lock};
    applet->home_button_long_pressed_blocked = true;
    applet->home_button_short_pressed_blocked = true;
    applet->home_button_double_click_enabled = true;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IApplicationFunctions::EndBlockingHomeButton(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    std::scoped_lock lk{applet->lock};
    applet->home_button_long_pressed_blocked = false;
    applet->home_button_short_pressed_blocked = false;
    applet->home_button_double_click_enabled = false;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IApplicationFunctions::PopLaunchParameter(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto kind = rp.PopEnum<LaunchParameterKind>();

    LOG_INFO(Service_AM, "called, kind={:08X}", kind);

    std::scoped_lock lk{applet->lock};

    auto& channel = kind == LaunchParameterKind::UserChannel
                        ? applet->user_channel_launch_parameter
                        : applet->preselected_user_launch_parameter;

    if (channel.empty()) {
        LOG_WARNING(Service_AM, "Attempted to pop parameter {} but none was found!", kind);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(AM::ResultNoDataInChannel);
        return;
    }

    auto data = channel.back();
    channel.pop_back();

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IStorage>(system, std::move(data));
}

void IApplicationFunctions::CreateApplicationAndRequestToStartForQuest(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IApplicationFunctions::EnsureSaveData(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    u128 user_id = rp.PopRaw<u128>();

    LOG_DEBUG(Service_AM, "called, uid={:016X}{:016X}", user_id[1], user_id[0]);

    FileSys::SaveDataAttribute attribute{};
    attribute.title_id = applet->program_id;
    attribute.user_id = user_id;
    attribute.type = FileSys::SaveDataType::SaveData;

    FileSys::VirtualDir save_data{};
    const auto res = system.GetFileSystemController().OpenSaveDataController()->CreateSaveData(
        &save_data, FileSys::SaveDataSpaceId::NandUser, attribute);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(res);
    rb.Push<u64>(0);
}

void IApplicationFunctions::SetTerminateResult(HLERequestContext& ctx) {
    // Takes an input u32 Result, no output.
    // For example, in some cases official apps use this with error 0x2A2 then
    // uses svcBreak.

    IPC::RequestParser rp{ctx};
    u32 result = rp.Pop<u32>();
    LOG_WARNING(Service_AM, "(STUBBED) called, result=0x{:08X}", result);

    std::scoped_lock lk{applet->lock};
    applet->terminate_result = Result(result);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IApplicationFunctions::GetDisplayVersion(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    std::array<u8, 0x10> version_string{};

    const auto res = [this] {
        const FileSys::PatchManager pm{applet->program_id, system.GetFileSystemController(),
                                       system.GetContentProvider()};
        auto metadata = pm.GetControlMetadata();
        if (metadata.first != nullptr) {
            return metadata;
        }

        const FileSys::PatchManager pm_update{FileSys::GetUpdateTitleID(applet->program_id),
                                              system.GetFileSystemController(),
                                              system.GetContentProvider()};
        return pm_update.GetControlMetadata();
    }();

    if (res.first != nullptr) {
        const auto& version = res.first->GetVersionString();
        std::copy(version.begin(), version.end(), version_string.begin());
    } else {
        static constexpr char default_version[]{"1.0.0"};
        std::memcpy(version_string.data(), default_version, sizeof(default_version));
    }

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(ResultSuccess);
    rb.PushRaw(version_string);
}

void IApplicationFunctions::GetDesiredLanguage(HLERequestContext& ctx) {
    // TODO(bunnei): This should be configurable
    LOG_DEBUG(Service_AM, "called");

    // Get supported languages from NACP, if possible
    // Default to 0 (all languages supported)
    u32 supported_languages = 0;

    const auto res = [this] {
        const FileSys::PatchManager pm{applet->program_id, system.GetFileSystemController(),
                                       system.GetContentProvider()};
        auto metadata = pm.GetControlMetadata();
        if (metadata.first != nullptr) {
            return metadata;
        }

        const FileSys::PatchManager pm_update{FileSys::GetUpdateTitleID(applet->program_id),
                                              system.GetFileSystemController(),
                                              system.GetContentProvider()};
        return pm_update.GetControlMetadata();
    }();

    if (res.first != nullptr) {
        supported_languages = res.first->GetSupportedLanguages();
    }

    // Call IApplicationManagerInterface implementation.
    auto& service_manager = system.ServiceManager();
    auto ns_am2 = service_manager.GetService<NS::NS>("ns:am2");
    auto app_man = ns_am2->GetApplicationManagerInterface();

    // Get desired application language
    u8 desired_language{};
    const auto res_lang =
        app_man->GetApplicationDesiredLanguage(&desired_language, supported_languages);
    if (res_lang != ResultSuccess) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(res_lang);
        return;
    }

    // Convert to settings language code.
    u64 language_code{};
    const auto res_code =
        app_man->ConvertApplicationLanguageToLanguageCode(&language_code, desired_language);
    if (res_code != ResultSuccess) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(res_code);
        return;
    }

    LOG_DEBUG(Service_AM, "got desired_language={:016X}", language_code);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push(language_code);
}

void IApplicationFunctions::IsGamePlayRecordingSupported(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(applet->gameplay_recording_supported);
}

void IApplicationFunctions::InitializeGamePlayRecording(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IApplicationFunctions::SetGamePlayRecordingState(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::RequestParser rp{ctx};

    std::scoped_lock lk{applet->lock};
    applet->gameplay_recording_state = rp.PopRaw<GameplayRecordingState>();

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IApplicationFunctions::NotifyRunning(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    std::scoped_lock lk{applet->lock};
    applet->is_running = true;

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u8>(0); // Unknown, seems to be ignored by official processes
}

void IApplicationFunctions::GetPseudoDeviceId(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(ResultSuccess);

    // Returns a 128-bit UUID
    rb.Push<u64>(0);
    rb.Push<u64>(0);
}

void IApplicationFunctions::ExtendSaveData(HLERequestContext& ctx) {
    struct Parameters {
        FileSys::SaveDataType type;
        u128 user_id;
        u64 new_normal_size;
        u64 new_journal_size;
    };
    static_assert(sizeof(Parameters) == 40);

    IPC::RequestParser rp{ctx};
    const auto [type, user_id, new_normal_size, new_journal_size] = rp.PopRaw<Parameters>();

    LOG_DEBUG(Service_AM,
              "called with type={:02X}, user_id={:016X}{:016X}, new_normal={:016X}, "
              "new_journal={:016X}",
              static_cast<u8>(type), user_id[1], user_id[0], new_normal_size, new_journal_size);

    system.GetFileSystemController().OpenSaveDataController()->WriteSaveDataSize(
        type, applet->program_id, user_id, {new_normal_size, new_journal_size});

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);

    // The following value is used upon failure to help the system recover.
    // Since we always succeed, this should be 0.
    rb.Push<u64>(0);
}

void IApplicationFunctions::GetSaveDataSize(HLERequestContext& ctx) {
    struct Parameters {
        FileSys::SaveDataType type;
        u128 user_id;
    };
    static_assert(sizeof(Parameters) == 24);

    IPC::RequestParser rp{ctx};
    const auto [type, user_id] = rp.PopRaw<Parameters>();

    LOG_DEBUG(Service_AM, "called with type={:02X}, user_id={:016X}{:016X}", type, user_id[1],
              user_id[0]);

    const auto size = system.GetFileSystemController().OpenSaveDataController()->ReadSaveDataSize(
        type, applet->program_id, user_id);

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(ResultSuccess);
    rb.Push(size.normal);
    rb.Push(size.journal);
}

void IApplicationFunctions::CreateCacheStorage(HLERequestContext& ctx) {
    struct InputParameters {
        u16 index;
        s64 size;
        s64 journal_size;
    };
    static_assert(sizeof(InputParameters) == 24);

    struct OutputParameters {
        u32 storage_target;
        u64 required_size;
    };
    static_assert(sizeof(OutputParameters) == 16);

    IPC::RequestParser rp{ctx};
    const auto params = rp.PopRaw<InputParameters>();

    LOG_WARNING(Service_AM, "(STUBBED) called with index={}, size={:#x}, journal_size={:#x}",
                params.index, params.size, params.journal_size);

    const OutputParameters resp{
        .storage_target = 1,
        .required_size = 0,
    };

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(ResultSuccess);
    rb.PushRaw(resp);
}

void IApplicationFunctions::GetSaveDataSizeMax(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    constexpr u64 size_max_normal = 0xFFFFFFF;
    constexpr u64 size_max_journal = 0xFFFFFFF;

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(ResultSuccess);
    rb.Push(size_max_normal);
    rb.Push(size_max_journal);
}

void IApplicationFunctions::QueryApplicationPlayStatistics(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u32>(0);
}

void IApplicationFunctions::QueryApplicationPlayStatisticsByUid(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u32>(0);
}

void IApplicationFunctions::ExecuteProgram(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::RequestParser rp{ctx};
    [[maybe_unused]] const auto unk_1 = rp.Pop<u32>();
    [[maybe_unused]] const auto unk_2 = rp.Pop<u32>();
    const auto program_index = rp.Pop<u64>();

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);

    // Swap user channel ownership into the system so that it will be preserved
    system.GetUserChannel().swap(applet->user_channel_launch_parameter);
    system.ExecuteProgram(program_index);
}

void IApplicationFunctions::ClearUserChannel(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    applet->user_channel_launch_parameter.clear();

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IApplicationFunctions::UnpopToUserChannel(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::RequestParser rp{ctx};
    const auto storage = rp.PopIpcInterface<IStorage>().lock();
    if (storage) {
        applet->user_channel_launch_parameter.push_back(storage->GetData());
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IApplicationFunctions::GetPreviousProgramIndex(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<s32>(applet->previous_program_index);
}

void IApplicationFunctions::GetGpuErrorDetectedSystemEvent(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(applet->gpu_error_detected_event.GetHandle());
}

void IApplicationFunctions::GetFriendInvitationStorageChannelEvent(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(applet->friend_invitation_storage_channel_event.GetHandle());
}

void IApplicationFunctions::TryPopFromFriendInvitationStorageChannel(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(AM::ResultNoDataInChannel);
}

void IApplicationFunctions::GetNotificationStorageChannelEvent(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(applet->notification_storage_channel_event.GetHandle());
}

void IApplicationFunctions::GetHealthWarningDisappearedSystemEvent(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(applet->health_warning_disappeared_system_event.GetHandle());
}

void IApplicationFunctions::PrepareForJit(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    std::scoped_lock lk{applet->lock};
    applet->jit_service_launched = true;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

} // namespace Service::AM
