// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/nca_metadata.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/romfs.h"
#include "core/file_sys/system_archive/system_archive.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/set/set.h"
#include "core/hle/service/set/set_sys.h"

namespace Service::Set {

namespace {
constexpr u64 SYSTEM_VERSION_FILE_MINOR_REVISION_OFFSET = 0x05;

enum class GetFirmwareVersionType {
    Version1,
    Version2,
};

void GetFirmwareVersionImpl(Core::System& system, HLERequestContext& ctx,
                            GetFirmwareVersionType type) {
    ASSERT_MSG(ctx.GetWriteBufferSize() == 0x100,
               "FirmwareVersion output buffer must be 0x100 bytes in size!");

    constexpr u64 FirmwareVersionSystemDataId = 0x0100000000000809;
    auto& fsc = system.GetFileSystemController();

    // Attempt to load version data from disk
    const FileSys::RegisteredCache* bis_system{};
    std::unique_ptr<FileSys::NCA> nca{};
    FileSys::VirtualDir romfs{};

    bis_system = fsc.GetSystemNANDContents();
    if (bis_system) {
        nca = bis_system->GetEntry(FirmwareVersionSystemDataId, FileSys::ContentRecordType::Data);
    }
    if (nca) {
        romfs = FileSys::ExtractRomFS(nca->GetRomFS());
    }
    if (!romfs) {
        romfs = FileSys::ExtractRomFS(
            FileSys::SystemArchive::SynthesizeSystemArchive(FirmwareVersionSystemDataId));
    }

    const auto early_exit_failure = [&ctx](std::string_view desc, Result code) {
        LOG_ERROR(Service_SET, "General failure while attempting to resolve firmware version ({}).",
                  desc);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(code);
    };

    const auto ver_file = romfs->GetFile("file");
    if (ver_file == nullptr) {
        early_exit_failure("The system version archive didn't contain the file 'file'.",
                           FileSys::ERROR_INVALID_ARGUMENT);
        return;
    }

    auto data = ver_file->ReadAllBytes();
    if (data.size() != 0x100) {
        early_exit_failure("The system version file 'file' was not the correct size.",
                           FileSys::ERROR_OUT_OF_BOUNDS);
        return;
    }

    // If the command is GetFirmwareVersion (as opposed to GetFirmwareVersion2), hardware will
    // zero out the REVISION_MINOR field.
    if (type == GetFirmwareVersionType::Version1) {
        data[SYSTEM_VERSION_FILE_MINOR_REVISION_OFFSET] = 0;
    }

    ctx.WriteBuffer(data);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}
} // Anonymous namespace

void SET_SYS::SetLanguageCode(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    language_code_setting = rp.PopEnum<LanguageCode>();

    LOG_INFO(Service_SET, "called, language_code={}", language_code_setting);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void SET_SYS::GetFirmwareVersion(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");
    GetFirmwareVersionImpl(system, ctx, GetFirmwareVersionType::Version1);
}

void SET_SYS::GetFirmwareVersion2(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");
    GetFirmwareVersionImpl(system, ctx, GetFirmwareVersionType::Version2);
}

void SET_SYS::GetAccountSettings(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushRaw(account_settings);
}

void SET_SYS::SetAccountSettings(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    account_settings = rp.PopRaw<AccountSettings>();

    LOG_INFO(Service_SET, "called, account_settings_flags={}", account_settings.flags);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void SET_SYS::GetEulaVersions(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    ctx.WriteBuffer(eula_versions);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(static_cast<u32>(eula_versions.size()));
}

void SET_SYS::SetEulaVersions(HLERequestContext& ctx) {
    const auto elements = ctx.GetReadBufferNumElements<EulaVersion>();
    const auto buffer_data = ctx.ReadBuffer();

    LOG_INFO(Service_SET, "called, elements={}", elements);

    eula_versions.resize(elements);
    for (std::size_t index = 0; index < elements; index++) {
        const std::size_t start_index = index * sizeof(EulaVersion);
        memcpy(eula_versions.data() + start_index, buffer_data.data() + start_index,
               sizeof(EulaVersion));
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void SET_SYS::GetColorSetId(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(color_set);
}

void SET_SYS::SetColorSetId(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    color_set = rp.PopEnum<ColorSet>();

    LOG_DEBUG(Service_SET, "called, color_set={}", color_set);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void SET_SYS::GetNotificationSettings(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    IPC::ResponseBuilder rb{ctx, 8};
    rb.Push(ResultSuccess);
    rb.PushRaw(notification_settings);
}

void SET_SYS::SetNotificationSettings(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    notification_settings = rp.PopRaw<NotificationSettings>();

    LOG_INFO(Service_SET, "called, flags={}, volume={}, head_time={}:{}, tailt_time={}:{}",
             notification_settings.flags.raw, notification_settings.volume,
             notification_settings.start_time.hour, notification_settings.start_time.minute,
             notification_settings.stop_time.hour, notification_settings.stop_time.minute);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void SET_SYS::GetAccountNotificationSettings(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    ctx.WriteBuffer(account_notifications);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(static_cast<u32>(account_notifications.size()));
}

void SET_SYS::SetAccountNotificationSettings(HLERequestContext& ctx) {
    const auto elements = ctx.GetReadBufferNumElements<AccountNotificationSettings>();
    const auto buffer_data = ctx.ReadBuffer();

    LOG_INFO(Service_SET, "called, elements={}", elements);

    account_notifications.resize(elements);
    for (std::size_t index = 0; index < elements; index++) {
        const std::size_t start_index = index * sizeof(AccountNotificationSettings);
        memcpy(account_notifications.data() + start_index, buffer_data.data() + start_index,
               sizeof(AccountNotificationSettings));
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

// FIXME: implement support for the real system_settings.ini

template <typename T>
static std::vector<u8> ToBytes(const T& value) {
    static_assert(std::is_trivially_copyable_v<T>);

    const auto* begin = reinterpret_cast<const u8*>(&value);
    const auto* end = begin + sizeof(T);

    return std::vector<u8>(begin, end);
}

using Settings =
    std::map<std::string, std::map<std::string, std::vector<u8>, std::less<>>, std::less<>>;

static Settings GetSettings() {
    Settings ret;

    ret["hbloader"]["applet_heap_size"] = ToBytes(u64{0x0});
    ret["hbloader"]["applet_heap_reservation_size"] = ToBytes(u64{0x8600000});

    return ret;
}

void SET_SYS::GetSettingsItemValueSize(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");

    // The category of the setting. This corresponds to the top-level keys of
    // system_settings.ini.
    const auto setting_category_buf{ctx.ReadBuffer(0)};
    const std::string setting_category{setting_category_buf.begin(), setting_category_buf.end()};

    // The name of the setting. This corresponds to the second-level keys of
    // system_settings.ini.
    const auto setting_name_buf{ctx.ReadBuffer(1)};
    const std::string setting_name{setting_name_buf.begin(), setting_name_buf.end()};

    auto settings{GetSettings()};
    u64 response_size{0};

    if (settings.contains(setting_category) && settings[setting_category].contains(setting_name)) {
        response_size = settings[setting_category][setting_name].size();
    }

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(response_size == 0 ? ResultUnknown : ResultSuccess);
    rb.Push(response_size);
}

void SET_SYS::GetSettingsItemValue(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");

    // The category of the setting. This corresponds to the top-level keys of
    // system_settings.ini.
    const auto setting_category_buf{ctx.ReadBuffer(0)};
    const std::string setting_category{setting_category_buf.begin(), setting_category_buf.end()};

    // The name of the setting. This corresponds to the second-level keys of
    // system_settings.ini.
    const auto setting_name_buf{ctx.ReadBuffer(1)};
    const std::string setting_name{setting_name_buf.begin(), setting_name_buf.end()};

    auto settings{GetSettings()};
    Result response{ResultUnknown};

    if (settings.contains(setting_category) && settings[setting_category].contains(setting_name)) {
        auto setting_value = settings[setting_category][setting_name];
        ctx.WriteBuffer(setting_value.data(), setting_value.size());
        response = ResultSuccess;
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(response);
}

void SET_SYS::GetTvSettings(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    IPC::ResponseBuilder rb{ctx, 10};
    rb.Push(ResultSuccess);
    rb.PushRaw(tv_settings);
}

void SET_SYS::SetTvSettings(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    tv_settings = rp.PopRaw<TvSettings>();

    LOG_INFO(Service_SET,
             "called, flags={}, cmu_mode={}, constrast_ratio={}, hdmi_content_type={}, "
             "rgb_range={}, tv_gama={}, tv_resolution={}, tv_underscan={}",
             tv_settings.flags.raw, tv_settings.cmu_mode, tv_settings.constrast_ratio,
             tv_settings.hdmi_content_type, tv_settings.rgb_range, tv_settings.tv_gama,
             tv_settings.tv_resolution, tv_settings.tv_underscan);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void SET_SYS::GetQuestFlag(HLERequestContext& ctx) {
    LOG_WARNING(Service_SET, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(QuestFlag::Retail);
}

void SET_SYS::SetRegionCode(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    region_code = rp.PopEnum<RegionCode>();

    LOG_INFO(Service_SET, "called, region_code={}", region_code);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void SET_SYS::GetPrimaryAlbumStorage(HLERequestContext& ctx) {
    LOG_WARNING(Service_SET, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(PrimaryAlbumStorage::SdCard);
}

void SET_SYS::GetSleepSettings(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    IPC::ResponseBuilder rb{ctx, 5};
    rb.Push(ResultSuccess);
    rb.PushRaw(sleep_settings);
}

void SET_SYS::SetSleepSettings(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    sleep_settings = rp.PopRaw<SleepSettings>();

    LOG_INFO(Service_SET, "called, flags={}, handheld_sleep_plan={}, console_sleep_plan={}",
             sleep_settings.flags.raw, sleep_settings.handheld_sleep_plan,
             sleep_settings.console_sleep_plan);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void SET_SYS::GetInitialLaunchSettings(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");
    IPC::ResponseBuilder rb{ctx, 10};
    rb.Push(ResultSuccess);
    rb.PushRaw(launch_settings);
}

void SET_SYS::SetInitialLaunchSettings(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    launch_settings = rp.PopRaw<InitialLaunchSettings>();

    LOG_INFO(Service_SET, "called, flags={}, timestamp={}", launch_settings.flags.raw,
             launch_settings.timestamp.time_point);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void SET_SYS::GetDeviceNickName(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");

    ctx.WriteBuffer(::Settings::values.device_name.GetValue());

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void SET_SYS::SetDeviceNickName(HLERequestContext& ctx) {
    const std::string device_name = Common::StringFromBuffer(ctx.ReadBuffer());

    LOG_INFO(Service_SET, "called, device_name={}", device_name);

    ::Settings::values.device_name = device_name;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void SET_SYS::GetProductModel(HLERequestContext& ctx) {
    const u32 product_model = 1;

    LOG_WARNING(Service_SET, "(STUBBED) called, product_model={}", product_model);
    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(product_model);
}

void SET_SYS::GetMiiAuthorId(HLERequestContext& ctx) {
    const auto author_id = Common::UUID::MakeDefault();

    LOG_WARNING(Service_SET, "(STUBBED) called, author_id={}", author_id.FormattedString());

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(ResultSuccess);
    rb.PushRaw(author_id);
}

void SET_SYS::GetAutoUpdateEnableFlag(HLERequestContext& ctx) {
    u8 auto_update_flag{};

    LOG_WARNING(Service_SET, "(STUBBED) called, auto_update_flag={}", auto_update_flag);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(auto_update_flag);
}

void SET_SYS::GetBatteryPercentageFlag(HLERequestContext& ctx) {
    u8 battery_percentage_flag{1};

    LOG_DEBUG(Service_SET, "(STUBBED) called, battery_percentage_flag={}", battery_percentage_flag);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(battery_percentage_flag);
}

void SET_SYS::GetErrorReportSharePermission(HLERequestContext& ctx) {
    LOG_WARNING(Service_SET, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(ErrorReportSharePermission::Denied);
}

void SET_SYS::GetAppletLaunchFlags(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called, applet_launch_flag={}", applet_launch_flag);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(applet_launch_flag);
}

void SET_SYS::SetAppletLaunchFlags(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    applet_launch_flag = rp.Pop<u32>();

    LOG_INFO(Service_SET, "called, applet_launch_flag={}", applet_launch_flag);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void SET_SYS::GetKeyboardLayout(HLERequestContext& ctx) {
    const auto language_code =
        available_language_codes[static_cast<s32>(::Settings::values.language_index.GetValue())];
    const auto key_code =
        std::find_if(language_to_layout.cbegin(), language_to_layout.cend(),
                     [=](const auto& element) { return element.first == language_code; });

    KeyboardLayout selected_keyboard_layout = KeyboardLayout::EnglishUs;
    if (key_code != language_to_layout.end()) {
        selected_keyboard_layout = key_code->second;
    }

    LOG_INFO(Service_SET, "called, selected_keyboard_layout={}", selected_keyboard_layout);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(static_cast<u32>(selected_keyboard_layout));
}

void SET_SYS::GetChineseTraditionalInputMethod(HLERequestContext& ctx) {
    LOG_WARNING(Service_SET, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(ChineseTraditionalInputMethod::Unknown0);
}

void SET_SYS::GetHomeMenuScheme(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "(STUBBED) called");

    const HomeMenuScheme default_color = {
        .main = 0xFF323232,
        .back = 0xFF323232,
        .sub = 0xFFFFFFFF,
        .bezel = 0xFFFFFFFF,
        .extra = 0xFF000000,
    };

    IPC::ResponseBuilder rb{ctx, 7};
    rb.Push(ResultSuccess);
    rb.PushRaw(default_color);
}

void SET_SYS::GetHomeMenuSchemeModel(HLERequestContext& ctx) {
    LOG_WARNING(Service_SET, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(0);
}
void SET_SYS::GetFieldTestingFlag(HLERequestContext& ctx) {
    LOG_WARNING(Service_SET, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u8>(false);
}

SET_SYS::SET_SYS(Core::System& system_) : ServiceFramework{system_, "set:sys"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &SET_SYS::SetLanguageCode, "SetLanguageCode"},
        {1, nullptr, "SetNetworkSettings"},
        {2, nullptr, "GetNetworkSettings"},
        {3, &SET_SYS::GetFirmwareVersion, "GetFirmwareVersion"},
        {4, &SET_SYS::GetFirmwareVersion2, "GetFirmwareVersion2"},
        {5, nullptr, "GetFirmwareVersionDigest"},
        {7, nullptr, "GetLockScreenFlag"},
        {8, nullptr, "SetLockScreenFlag"},
        {9, nullptr, "GetBacklightSettings"},
        {10, nullptr, "SetBacklightSettings"},
        {11, nullptr, "SetBluetoothDevicesSettings"},
        {12, nullptr, "GetBluetoothDevicesSettings"},
        {13, nullptr, "GetExternalSteadyClockSourceId"},
        {14, nullptr, "SetExternalSteadyClockSourceId"},
        {15, nullptr, "GetUserSystemClockContext"},
        {16, nullptr, "SetUserSystemClockContext"},
        {17, &SET_SYS::GetAccountSettings, "GetAccountSettings"},
        {18, &SET_SYS::SetAccountSettings, "SetAccountSettings"},
        {19, nullptr, "GetAudioVolume"},
        {20, nullptr, "SetAudioVolume"},
        {21, &SET_SYS::GetEulaVersions, "GetEulaVersions"},
        {22, &SET_SYS::SetEulaVersions, "SetEulaVersions"},
        {23, &SET_SYS::GetColorSetId, "GetColorSetId"},
        {24, &SET_SYS::SetColorSetId, "SetColorSetId"},
        {25, nullptr, "GetConsoleInformationUploadFlag"},
        {26, nullptr, "SetConsoleInformationUploadFlag"},
        {27, nullptr, "GetAutomaticApplicationDownloadFlag"},
        {28, nullptr, "SetAutomaticApplicationDownloadFlag"},
        {29, &SET_SYS::GetNotificationSettings, "GetNotificationSettings"},
        {30, &SET_SYS::SetNotificationSettings, "SetNotificationSettings"},
        {31, &SET_SYS::GetAccountNotificationSettings, "GetAccountNotificationSettings"},
        {32, &SET_SYS::SetAccountNotificationSettings, "SetAccountNotificationSettings"},
        {35, nullptr, "GetVibrationMasterVolume"},
        {36, nullptr, "SetVibrationMasterVolume"},
        {37, &SET_SYS::GetSettingsItemValueSize, "GetSettingsItemValueSize"},
        {38, &SET_SYS::GetSettingsItemValue, "GetSettingsItemValue"},
        {39, &SET_SYS::GetTvSettings, "GetTvSettings"},
        {40, &SET_SYS::SetTvSettings, "SetTvSettings"},
        {41, nullptr, "GetEdid"},
        {42, nullptr, "SetEdid"},
        {43, nullptr, "GetAudioOutputMode"},
        {44, nullptr, "SetAudioOutputMode"},
        {45, nullptr, "IsForceMuteOnHeadphoneRemoved"},
        {46, nullptr, "SetForceMuteOnHeadphoneRemoved"},
        {47, &SET_SYS::GetQuestFlag, "GetQuestFlag"},
        {48, nullptr, "SetQuestFlag"},
        {49, nullptr, "GetDataDeletionSettings"},
        {50, nullptr, "SetDataDeletionSettings"},
        {51, nullptr, "GetInitialSystemAppletProgramId"},
        {52, nullptr, "GetOverlayDispProgramId"},
        {53, nullptr, "GetDeviceTimeZoneLocationName"},
        {54, nullptr, "SetDeviceTimeZoneLocationName"},
        {55, nullptr, "GetWirelessCertificationFileSize"},
        {56, nullptr, "GetWirelessCertificationFile"},
        {57, &SET_SYS::SetRegionCode, "SetRegionCode"},
        {58, nullptr, "GetNetworkSystemClockContext"},
        {59, nullptr, "SetNetworkSystemClockContext"},
        {60, nullptr, "IsUserSystemClockAutomaticCorrectionEnabled"},
        {61, nullptr, "SetUserSystemClockAutomaticCorrectionEnabled"},
        {62, nullptr, "GetDebugModeFlag"},
        {63, &SET_SYS::GetPrimaryAlbumStorage, "GetPrimaryAlbumStorage"},
        {64, nullptr, "SetPrimaryAlbumStorage"},
        {65, nullptr, "GetUsb30EnableFlag"},
        {66, nullptr, "SetUsb30EnableFlag"},
        {67, nullptr, "GetBatteryLot"},
        {68, nullptr, "GetSerialNumber"},
        {69, nullptr, "GetNfcEnableFlag"},
        {70, nullptr, "SetNfcEnableFlag"},
        {71, &SET_SYS::GetSleepSettings, "GetSleepSettings"},
        {72, &SET_SYS::SetSleepSettings, "SetSleepSettings"},
        {73, nullptr, "GetWirelessLanEnableFlag"},
        {74, nullptr, "SetWirelessLanEnableFlag"},
        {75, &SET_SYS::GetInitialLaunchSettings, "GetInitialLaunchSettings"},
        {76, &SET_SYS::SetInitialLaunchSettings, "SetInitialLaunchSettings"},
        {77, &SET_SYS::GetDeviceNickName, "GetDeviceNickName"},
        {78, &SET_SYS::SetDeviceNickName, "SetDeviceNickName"},
        {79, &SET_SYS::GetProductModel, "GetProductModel"},
        {80, nullptr, "GetLdnChannel"},
        {81, nullptr, "SetLdnChannel"},
        {82, nullptr, "AcquireTelemetryDirtyFlagEventHandle"},
        {83, nullptr, "GetTelemetryDirtyFlags"},
        {84, nullptr, "GetPtmBatteryLot"},
        {85, nullptr, "SetPtmBatteryLot"},
        {86, nullptr, "GetPtmFuelGaugeParameter"},
        {87, nullptr, "SetPtmFuelGaugeParameter"},
        {88, nullptr, "GetBluetoothEnableFlag"},
        {89, nullptr, "SetBluetoothEnableFlag"},
        {90, &SET_SYS::GetMiiAuthorId, "GetMiiAuthorId"},
        {91, nullptr, "SetShutdownRtcValue"},
        {92, nullptr, "GetShutdownRtcValue"},
        {93, nullptr, "AcquireFatalDirtyFlagEventHandle"},
        {94, nullptr, "GetFatalDirtyFlags"},
        {95, &SET_SYS::GetAutoUpdateEnableFlag, "GetAutoUpdateEnableFlag"},
        {96, nullptr, "SetAutoUpdateEnableFlag"},
        {97, nullptr, "GetNxControllerSettings"},
        {98, nullptr, "SetNxControllerSettings"},
        {99, &SET_SYS::GetBatteryPercentageFlag, "GetBatteryPercentageFlag"},
        {100, nullptr, "SetBatteryPercentageFlag"},
        {101, nullptr, "GetExternalRtcResetFlag"},
        {102, nullptr, "SetExternalRtcResetFlag"},
        {103, nullptr, "GetUsbFullKeyEnableFlag"},
        {104, nullptr, "SetUsbFullKeyEnableFlag"},
        {105, nullptr, "SetExternalSteadyClockInternalOffset"},
        {106, nullptr, "GetExternalSteadyClockInternalOffset"},
        {107, nullptr, "GetBacklightSettingsEx"},
        {108, nullptr, "SetBacklightSettingsEx"},
        {109, nullptr, "GetHeadphoneVolumeWarningCount"},
        {110, nullptr, "SetHeadphoneVolumeWarningCount"},
        {111, nullptr, "GetBluetoothAfhEnableFlag"},
        {112, nullptr, "SetBluetoothAfhEnableFlag"},
        {113, nullptr, "GetBluetoothBoostEnableFlag"},
        {114, nullptr, "SetBluetoothBoostEnableFlag"},
        {115, nullptr, "GetInRepairProcessEnableFlag"},
        {116, nullptr, "SetInRepairProcessEnableFlag"},
        {117, nullptr, "GetHeadphoneVolumeUpdateFlag"},
        {118, nullptr, "SetHeadphoneVolumeUpdateFlag"},
        {119, nullptr, "NeedsToUpdateHeadphoneVolume"},
        {120, nullptr, "GetPushNotificationActivityModeOnSleep"},
        {121, nullptr, "SetPushNotificationActivityModeOnSleep"},
        {122, nullptr, "GetServiceDiscoveryControlSettings"},
        {123, nullptr, "SetServiceDiscoveryControlSettings"},
        {124, &SET_SYS::GetErrorReportSharePermission, "GetErrorReportSharePermission"},
        {125, nullptr, "SetErrorReportSharePermission"},
        {126, &SET_SYS::GetAppletLaunchFlags, "GetAppletLaunchFlags"},
        {127, &SET_SYS::SetAppletLaunchFlags, "SetAppletLaunchFlags"},
        {128, nullptr, "GetConsoleSixAxisSensorAccelerationBias"},
        {129, nullptr, "SetConsoleSixAxisSensorAccelerationBias"},
        {130, nullptr, "GetConsoleSixAxisSensorAngularVelocityBias"},
        {131, nullptr, "SetConsoleSixAxisSensorAngularVelocityBias"},
        {132, nullptr, "GetConsoleSixAxisSensorAccelerationGain"},
        {133, nullptr, "SetConsoleSixAxisSensorAccelerationGain"},
        {134, nullptr, "GetConsoleSixAxisSensorAngularVelocityGain"},
        {135, nullptr, "SetConsoleSixAxisSensorAngularVelocityGain"},
        {136, &SET_SYS::GetKeyboardLayout, "GetKeyboardLayout"},
        {137, nullptr, "SetKeyboardLayout"},
        {138, nullptr, "GetWebInspectorFlag"},
        {139, nullptr, "GetAllowedSslHosts"},
        {140, nullptr, "GetHostFsMountPoint"},
        {141, nullptr, "GetRequiresRunRepairTimeReviser"},
        {142, nullptr, "SetRequiresRunRepairTimeReviser"},
        {143, nullptr, "SetBlePairingSettings"},
        {144, nullptr, "GetBlePairingSettings"},
        {145, nullptr, "GetConsoleSixAxisSensorAngularVelocityTimeBias"},
        {146, nullptr, "SetConsoleSixAxisSensorAngularVelocityTimeBias"},
        {147, nullptr, "GetConsoleSixAxisSensorAngularAcceleration"},
        {148, nullptr, "SetConsoleSixAxisSensorAngularAcceleration"},
        {149, nullptr, "GetRebootlessSystemUpdateVersion"},
        {150, nullptr, "GetDeviceTimeZoneLocationUpdatedTime"},
        {151, nullptr, "SetDeviceTimeZoneLocationUpdatedTime"},
        {152, nullptr, "GetUserSystemClockAutomaticCorrectionUpdatedTime"},
        {153, nullptr, "SetUserSystemClockAutomaticCorrectionUpdatedTime"},
        {154, nullptr, "GetAccountOnlineStorageSettings"},
        {155, nullptr, "SetAccountOnlineStorageSettings"},
        {156, nullptr, "GetPctlReadyFlag"},
        {157, nullptr, "SetPctlReadyFlag"},
        {158, nullptr, "GetAnalogStickUserCalibrationL"},
        {159, nullptr, "SetAnalogStickUserCalibrationL"},
        {160, nullptr, "GetAnalogStickUserCalibrationR"},
        {161, nullptr, "SetAnalogStickUserCalibrationR"},
        {162, nullptr, "GetPtmBatteryVersion"},
        {163, nullptr, "SetPtmBatteryVersion"},
        {164, nullptr, "GetUsb30HostEnableFlag"},
        {165, nullptr, "SetUsb30HostEnableFlag"},
        {166, nullptr, "GetUsb30DeviceEnableFlag"},
        {167, nullptr, "SetUsb30DeviceEnableFlag"},
        {168, nullptr, "GetThemeId"},
        {169, nullptr, "SetThemeId"},
        {170, &SET_SYS::GetChineseTraditionalInputMethod, "GetChineseTraditionalInputMethod"},
        {171, nullptr, "SetChineseTraditionalInputMethod"},
        {172, nullptr, "GetPtmCycleCountReliability"},
        {173, nullptr, "SetPtmCycleCountReliability"},
        {174, &SET_SYS::GetHomeMenuScheme, "GetHomeMenuScheme"},
        {175, nullptr, "GetThemeSettings"},
        {176, nullptr, "SetThemeSettings"},
        {177, nullptr, "GetThemeKey"},
        {178, nullptr, "SetThemeKey"},
        {179, nullptr, "GetZoomFlag"},
        {180, nullptr, "SetZoomFlag"},
        {181, nullptr, "GetT"},
        {182, nullptr, "SetT"},
        {183, nullptr, "GetPlatformRegion"},
        {184, nullptr, "SetPlatformRegion"},
        {185, &SET_SYS::GetHomeMenuSchemeModel, "GetHomeMenuSchemeModel"},
        {186, nullptr, "GetMemoryUsageRateFlag"},
        {187, nullptr, "GetTouchScreenMode"},
        {188, nullptr, "SetTouchScreenMode"},
        {189, nullptr, "GetButtonConfigSettingsFull"},
        {190, nullptr, "SetButtonConfigSettingsFull"},
        {191, nullptr, "GetButtonConfigSettingsEmbedded"},
        {192, nullptr, "SetButtonConfigSettingsEmbedded"},
        {193, nullptr, "GetButtonConfigSettingsLeft"},
        {194, nullptr, "SetButtonConfigSettingsLeft"},
        {195, nullptr, "GetButtonConfigSettingsRight"},
        {196, nullptr, "SetButtonConfigSettingsRight"},
        {197, nullptr, "GetButtonConfigRegisteredSettingsEmbedded"},
        {198, nullptr, "SetButtonConfigRegisteredSettingsEmbedded"},
        {199, nullptr, "GetButtonConfigRegisteredSettings"},
        {200, nullptr, "SetButtonConfigRegisteredSettings"},
        {201, &SET_SYS::GetFieldTestingFlag, "GetFieldTestingFlag"},
        {202, nullptr, "SetFieldTestingFlag"},
        {203, nullptr, "GetPanelCrcMode"},
        {204, nullptr, "SetPanelCrcMode"},
        {205, nullptr, "GetNxControllerSettingsEx"},
        {206, nullptr, "SetNxControllerSettingsEx"},
        {207, nullptr, "GetHearingProtectionSafeguardFlag"},
        {208, nullptr, "SetHearingProtectionSafeguardFlag"},
        {209, nullptr, "GetHearingProtectionSafeguardRemainingTime"},
        {210, nullptr, "SetHearingProtectionSafeguardRemainingTime"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

SET_SYS::~SET_SYS() = default;

} // namespace Service::Set
