// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <fstream>

#include "common/assert.h"
#include "common/fs/file.h"
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
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
#include "core/hle/service/set/settings_server.h"
#include "core/hle/service/set/system_settings_server.h"

namespace Service::Set {

namespace {
constexpr u32 SETTINGS_VERSION{1u};
constexpr auto SETTINGS_MAGIC = Common::MakeMagic('y', 'u', 'z', 'u', '_', 's', 'e', 't');
struct SettingsHeader {
    u64 magic;
    u32 version;
    u32 reserved;
};
} // Anonymous namespace

Result GetFirmwareVersionImpl(FirmwareVersionFormat& out_firmware, Core::System& system,
                              GetFirmwareVersionType type) {
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
        if (auto nca_romfs = nca->GetRomFS(); nca_romfs) {
            romfs = FileSys::ExtractRomFS(nca_romfs);
        }
    }
    if (!romfs) {
        romfs = FileSys::ExtractRomFS(
            FileSys::SystemArchive::SynthesizeSystemArchive(FirmwareVersionSystemDataId));
    }

    const auto early_exit_failure = [](std::string_view desc, Result code) {
        LOG_ERROR(Service_SET, "General failure while attempting to resolve firmware version ({}).",
                  desc);
        return code;
    };

    const auto ver_file = romfs->GetFile("file");
    if (ver_file == nullptr) {
        return early_exit_failure("The system version archive didn't contain the file 'file'.",
                                  FileSys::ERROR_INVALID_ARGUMENT);
    }

    auto data = ver_file->ReadAllBytes();
    if (data.size() != sizeof(FirmwareVersionFormat)) {
        return early_exit_failure("The system version file 'file' was not the correct size.",
                                  FileSys::ERROR_OUT_OF_BOUNDS);
    }

    std::memcpy(&out_firmware, data.data(), sizeof(FirmwareVersionFormat));

    // If the command is GetFirmwareVersion (as opposed to GetFirmwareVersion2), hardware will
    // zero out the REVISION_MINOR field.
    if (type == GetFirmwareVersionType::Version1) {
        out_firmware.revision_minor = 0;
    }

    return ResultSuccess;
}

ISystemSettingsServer::ISystemSettingsServer(Core::System& system_)
    : ServiceFramework{system_, "set:sys"}, m_system{system} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &ISystemSettingsServer::SetLanguageCode, "SetLanguageCode"},
        {1, nullptr, "SetNetworkSettings"},
        {2, nullptr, "GetNetworkSettings"},
        {3, &ISystemSettingsServer::GetFirmwareVersion, "GetFirmwareVersion"},
        {4, &ISystemSettingsServer::GetFirmwareVersion2, "GetFirmwareVersion2"},
        {5, nullptr, "GetFirmwareVersionDigest"},
        {7, &ISystemSettingsServer::GetLockScreenFlag, "GetLockScreenFlag"},
        {8, &ISystemSettingsServer::SetLockScreenFlag, "SetLockScreenFlag"},
        {9, nullptr, "GetBacklightSettings"},
        {10, nullptr, "SetBacklightSettings"},
        {11, nullptr, "SetBluetoothDevicesSettings"},
        {12, nullptr, "GetBluetoothDevicesSettings"},
        {13, &ISystemSettingsServer::GetExternalSteadyClockSourceId, "GetExternalSteadyClockSourceId"},
        {14, &ISystemSettingsServer::SetExternalSteadyClockSourceId, "SetExternalSteadyClockSourceId"},
        {15, &ISystemSettingsServer::GetUserSystemClockContext, "GetUserSystemClockContext"},
        {16, &ISystemSettingsServer::SetUserSystemClockContext, "SetUserSystemClockContext"},
        {17, &ISystemSettingsServer::GetAccountSettings, "GetAccountSettings"},
        {18, &ISystemSettingsServer::SetAccountSettings, "SetAccountSettings"},
        {19, nullptr, "GetAudioVolume"},
        {20, nullptr, "SetAudioVolume"},
        {21, &ISystemSettingsServer::GetEulaVersions, "GetEulaVersions"},
        {22, &ISystemSettingsServer::SetEulaVersions, "SetEulaVersions"},
        {23, &ISystemSettingsServer::GetColorSetId, "GetColorSetId"},
        {24, &ISystemSettingsServer::SetColorSetId, "SetColorSetId"},
        {25, nullptr, "GetConsoleInformationUploadFlag"},
        {26, nullptr, "SetConsoleInformationUploadFlag"},
        {27, nullptr, "GetAutomaticApplicationDownloadFlag"},
        {28, nullptr, "SetAutomaticApplicationDownloadFlag"},
        {29, &ISystemSettingsServer::GetNotificationSettings, "GetNotificationSettings"},
        {30, &ISystemSettingsServer::SetNotificationSettings, "SetNotificationSettings"},
        {31, &ISystemSettingsServer::GetAccountNotificationSettings, "GetAccountNotificationSettings"},
        {32, &ISystemSettingsServer::SetAccountNotificationSettings, "SetAccountNotificationSettings"},
        {35, nullptr, "GetVibrationMasterVolume"},
        {36, nullptr, "SetVibrationMasterVolume"},
        {37, &ISystemSettingsServer::GetSettingsItemValueSize, "GetSettingsItemValueSize"},
        {38, &ISystemSettingsServer::GetSettingsItemValue, "GetSettingsItemValue"},
        {39, &ISystemSettingsServer::GetTvSettings, "GetTvSettings"},
        {40, &ISystemSettingsServer::SetTvSettings, "SetTvSettings"},
        {41, nullptr, "GetEdid"},
        {42, nullptr, "SetEdid"},
        {43, nullptr, "GetAudioOutputMode"},
        {44, nullptr, "SetAudioOutputMode"},
        {45, nullptr, "IsForceMuteOnHeadphoneRemoved"},
        {46, nullptr, "SetForceMuteOnHeadphoneRemoved"},
        {47, &ISystemSettingsServer::GetQuestFlag, "GetQuestFlag"},
        {48, nullptr, "SetQuestFlag"},
        {49, nullptr, "GetDataDeletionSettings"},
        {50, nullptr, "SetDataDeletionSettings"},
        {51, nullptr, "GetInitialSystemAppletProgramId"},
        {52, nullptr, "GetOverlayDispProgramId"},
        {53, &ISystemSettingsServer::GetDeviceTimeZoneLocationName, "GetDeviceTimeZoneLocationName"},
        {54, &ISystemSettingsServer::SetDeviceTimeZoneLocationName, "SetDeviceTimeZoneLocationName"},
        {55, nullptr, "GetWirelessCertificationFileSize"},
        {56, nullptr, "GetWirelessCertificationFile"},
        {57, &ISystemSettingsServer::SetRegionCode, "SetRegionCode"},
        {58, &ISystemSettingsServer::GetNetworkSystemClockContext, "GetNetworkSystemClockContext"},
        {59, &ISystemSettingsServer::SetNetworkSystemClockContext, "SetNetworkSystemClockContext"},
        {60, &ISystemSettingsServer::IsUserSystemClockAutomaticCorrectionEnabled, "IsUserSystemClockAutomaticCorrectionEnabled"},
        {61, &ISystemSettingsServer::SetUserSystemClockAutomaticCorrectionEnabled, "SetUserSystemClockAutomaticCorrectionEnabled"},
        {62, &ISystemSettingsServer::GetDebugModeFlag, "GetDebugModeFlag"},
        {63, &ISystemSettingsServer::GetPrimaryAlbumStorage, "GetPrimaryAlbumStorage"},
        {64, nullptr, "SetPrimaryAlbumStorage"},
        {65, nullptr, "GetUsb30EnableFlag"},
        {66, nullptr, "SetUsb30EnableFlag"},
        {67, nullptr, "GetBatteryLot"},
        {68, nullptr, "GetSerialNumber"},
        {69, &ISystemSettingsServer::GetNfcEnableFlag, "GetNfcEnableFlag"},
        {70, &ISystemSettingsServer::SetNfcEnableFlag, "SetNfcEnableFlag"},
        {71, &ISystemSettingsServer::GetSleepSettings, "GetSleepSettings"},
        {72, &ISystemSettingsServer::SetSleepSettings, "SetSleepSettings"},
        {73, &ISystemSettingsServer::GetWirelessLanEnableFlag, "GetWirelessLanEnableFlag"},
        {74, &ISystemSettingsServer::SetWirelessLanEnableFlag, "SetWirelessLanEnableFlag"},
        {75, &ISystemSettingsServer::GetInitialLaunchSettings, "GetInitialLaunchSettings"},
        {76, &ISystemSettingsServer::SetInitialLaunchSettings, "SetInitialLaunchSettings"},
        {77, &ISystemSettingsServer::GetDeviceNickName, "GetDeviceNickName"},
        {78, &ISystemSettingsServer::SetDeviceNickName, "SetDeviceNickName"},
        {79, &ISystemSettingsServer::GetProductModel, "GetProductModel"},
        {80, nullptr, "GetLdnChannel"},
        {81, nullptr, "SetLdnChannel"},
        {82, nullptr, "AcquireTelemetryDirtyFlagEventHandle"},
        {83, nullptr, "GetTelemetryDirtyFlags"},
        {84, nullptr, "GetPtmBatteryLot"},
        {85, nullptr, "SetPtmBatteryLot"},
        {86, nullptr, "GetPtmFuelGaugeParameter"},
        {87, nullptr, "SetPtmFuelGaugeParameter"},
        {88, &ISystemSettingsServer::GetBluetoothEnableFlag, "GetBluetoothEnableFlag"},
        {89, &ISystemSettingsServer::SetBluetoothEnableFlag, "SetBluetoothEnableFlag"},
        {90, &ISystemSettingsServer::GetMiiAuthorId, "GetMiiAuthorId"},
        {91, nullptr, "SetShutdownRtcValue"},
        {92, nullptr, "GetShutdownRtcValue"},
        {93, nullptr, "AcquireFatalDirtyFlagEventHandle"},
        {94, nullptr, "GetFatalDirtyFlags"},
        {95, &ISystemSettingsServer::GetAutoUpdateEnableFlag, "GetAutoUpdateEnableFlag"},
        {96, nullptr, "SetAutoUpdateEnableFlag"},
        {97, nullptr, "GetNxControllerSettings"},
        {98, nullptr, "SetNxControllerSettings"},
        {99, &ISystemSettingsServer::GetBatteryPercentageFlag, "GetBatteryPercentageFlag"},
        {100, nullptr, "SetBatteryPercentageFlag"},
        {101, nullptr, "GetExternalRtcResetFlag"},
        {102, nullptr, "SetExternalRtcResetFlag"},
        {103, nullptr, "GetUsbFullKeyEnableFlag"},
        {104, nullptr, "SetUsbFullKeyEnableFlag"},
        {105, &ISystemSettingsServer::SetExternalSteadyClockInternalOffset, "SetExternalSteadyClockInternalOffset"},
        {106, &ISystemSettingsServer::GetExternalSteadyClockInternalOffset, "GetExternalSteadyClockInternalOffset"},
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
        {124, &ISystemSettingsServer::GetErrorReportSharePermission, "GetErrorReportSharePermission"},
        {125, nullptr, "SetErrorReportSharePermission"},
        {126, &ISystemSettingsServer::GetAppletLaunchFlags, "GetAppletLaunchFlags"},
        {127, &ISystemSettingsServer::SetAppletLaunchFlags, "SetAppletLaunchFlags"},
        {128, nullptr, "GetConsoleSixAxisSensorAccelerationBias"},
        {129, nullptr, "SetConsoleSixAxisSensorAccelerationBias"},
        {130, nullptr, "GetConsoleSixAxisSensorAngularVelocityBias"},
        {131, nullptr, "SetConsoleSixAxisSensorAngularVelocityBias"},
        {132, nullptr, "GetConsoleSixAxisSensorAccelerationGain"},
        {133, nullptr, "SetConsoleSixAxisSensorAccelerationGain"},
        {134, nullptr, "GetConsoleSixAxisSensorAngularVelocityGain"},
        {135, nullptr, "SetConsoleSixAxisSensorAngularVelocityGain"},
        {136, &ISystemSettingsServer::GetKeyboardLayout, "GetKeyboardLayout"},
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
        {150, &ISystemSettingsServer::GetDeviceTimeZoneLocationUpdatedTime, "GetDeviceTimeZoneLocationUpdatedTime"},
        {151, &ISystemSettingsServer::SetDeviceTimeZoneLocationUpdatedTime, "SetDeviceTimeZoneLocationUpdatedTime"},
        {152, &ISystemSettingsServer::GetUserSystemClockAutomaticCorrectionUpdatedTime, "GetUserSystemClockAutomaticCorrectionUpdatedTime"},
        {153, &ISystemSettingsServer::SetUserSystemClockAutomaticCorrectionUpdatedTime, "SetUserSystemClockAutomaticCorrectionUpdatedTime"},
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
        {170, &ISystemSettingsServer::GetChineseTraditionalInputMethod, "GetChineseTraditionalInputMethod"},
        {171, nullptr, "SetChineseTraditionalInputMethod"},
        {172, nullptr, "GetPtmCycleCountReliability"},
        {173, nullptr, "SetPtmCycleCountReliability"},
        {174, &ISystemSettingsServer::GetHomeMenuScheme, "GetHomeMenuScheme"},
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
        {185, &ISystemSettingsServer::GetHomeMenuSchemeModel, "GetHomeMenuSchemeModel"},
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
        {201, &ISystemSettingsServer::GetFieldTestingFlag, "GetFieldTestingFlag"},
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

    SetupSettings();
    m_save_thread =
        std::jthread([this](std::stop_token stop_token) { StoreSettingsThreadFunc(stop_token); });
}

ISystemSettingsServer::~ISystemSettingsServer() {
    SetSaveNeeded();
    m_save_thread.request_stop();
}

bool ISystemSettingsServer::LoadSettingsFile(std::filesystem::path& path, auto&& default_func) {
    using settings_type = decltype(default_func());

    if (!Common::FS::CreateDirs(path)) {
        return false;
    }

    auto settings_file = path / "settings.dat";
    auto exists = std::filesystem::exists(settings_file);
    auto file_size_ok = exists && std::filesystem::file_size(settings_file) ==
                                      sizeof(SettingsHeader) + sizeof(settings_type);

    auto ResetToDefault = [&]() {
        auto default_settings{default_func()};

        SettingsHeader hdr{
            .magic = SETTINGS_MAGIC,
            .version = SETTINGS_VERSION,
            .reserved = 0u,
        };

        std::ofstream out_settings_file(settings_file, std::ios::out | std::ios::binary);
        out_settings_file.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
        out_settings_file.write(reinterpret_cast<const char*>(&default_settings),
                                sizeof(settings_type));
        out_settings_file.flush();
        out_settings_file.close();
    };

    constexpr auto IsHeaderValid = [](std::ifstream& file) -> bool {
        if (!file.is_open()) {
            return false;
        }
        SettingsHeader hdr{};
        file.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
        return hdr.magic == SETTINGS_MAGIC && hdr.version == SETTINGS_VERSION;
    };

    if (!exists || !file_size_ok) {
        ResetToDefault();
    }

    std::ifstream file(settings_file, std::ios::binary | std::ios::in);
    if (!IsHeaderValid(file)) {
        file.close();
        ResetToDefault();
        file = std::ifstream(settings_file, std::ios::binary | std::ios::in);
        if (!IsHeaderValid(file)) {
            return false;
        }
    }

    if constexpr (std::is_same_v<settings_type, PrivateSettings>) {
        file.read(reinterpret_cast<char*>(&m_private_settings), sizeof(settings_type));
    } else if constexpr (std::is_same_v<settings_type, DeviceSettings>) {
        file.read(reinterpret_cast<char*>(&m_device_settings), sizeof(settings_type));
    } else if constexpr (std::is_same_v<settings_type, ApplnSettings>) {
        file.read(reinterpret_cast<char*>(&m_appln_settings), sizeof(settings_type));
    } else if constexpr (std::is_same_v<settings_type, SystemSettings>) {
        file.read(reinterpret_cast<char*>(&m_system_settings), sizeof(settings_type));
    } else {
        UNREACHABLE();
    }
    file.close();

    return true;
}

bool ISystemSettingsServer::StoreSettingsFile(std::filesystem::path& path, auto& settings) {
    using settings_type = std::decay_t<decltype(settings)>;

    if (!Common::FS::IsDir(path)) {
        return false;
    }

    auto settings_base = path / "settings";
    auto settings_tmp_file = settings_base;
    settings_tmp_file = settings_tmp_file.replace_extension("tmp");
    std::ofstream file(settings_tmp_file, std::ios::binary | std::ios::out);
    if (!file.is_open()) {
        return false;
    }

    SettingsHeader hdr{
        .magic = SETTINGS_MAGIC,
        .version = SETTINGS_VERSION,
        .reserved = 0u,
    };
    file.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));

    if constexpr (std::is_same_v<settings_type, PrivateSettings>) {
        file.write(reinterpret_cast<const char*>(&m_private_settings), sizeof(settings_type));
    } else if constexpr (std::is_same_v<settings_type, DeviceSettings>) {
        file.write(reinterpret_cast<const char*>(&m_device_settings), sizeof(settings_type));
    } else if constexpr (std::is_same_v<settings_type, ApplnSettings>) {
        file.write(reinterpret_cast<const char*>(&m_appln_settings), sizeof(settings_type));
    } else if constexpr (std::is_same_v<settings_type, SystemSettings>) {
        file.write(reinterpret_cast<const char*>(&m_system_settings), sizeof(settings_type));
    } else {
        UNREACHABLE();
    }
    file.close();

    std::filesystem::rename(settings_tmp_file, settings_base.replace_extension("dat"));

    return true;
}

void ISystemSettingsServer::SetLanguageCode(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    m_system_settings.language_code = rp.PopEnum<LanguageCode>();
    SetSaveNeeded();

    LOG_INFO(Service_SET, "called, language_code={}", m_system_settings.language_code);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISystemSettingsServer::GetFirmwareVersion(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");

    FirmwareVersionFormat firmware_data{};
    const auto result =
        GetFirmwareVersionImpl(firmware_data, system, GetFirmwareVersionType::Version1);

    if (result.IsSuccess()) {
        ctx.WriteBuffer(firmware_data);
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void ISystemSettingsServer::GetFirmwareVersion2(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");

    FirmwareVersionFormat firmware_data{};
    const auto result =
        GetFirmwareVersionImpl(firmware_data, system, GetFirmwareVersionType::Version2);

    if (result.IsSuccess()) {
        ctx.WriteBuffer(firmware_data);
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void ISystemSettingsServer::GetExternalSteadyClockSourceId(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    Common::UUID id{};
    auto res = GetExternalSteadyClockSourceId(id);

    IPC::ResponseBuilder rb{ctx, 2 + sizeof(Common::UUID) / sizeof(u32)};
    rb.Push(res);
    rb.PushRaw(id);
}

void ISystemSettingsServer::SetExternalSteadyClockSourceId(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    IPC::RequestParser rp{ctx};
    auto id{rp.PopRaw<Common::UUID>()};

    auto res = SetExternalSteadyClockSourceId(id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void ISystemSettingsServer::GetUserSystemClockContext(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    Service::Time::Clock::SystemClockContext context{};
    auto res = GetUserSystemClockContext(context);

    IPC::ResponseBuilder rb{ctx,
                            2 + sizeof(Service::Time::Clock::SystemClockContext) / sizeof(u32)};
    rb.Push(res);
    rb.PushRaw(context);
}

void ISystemSettingsServer::SetUserSystemClockContext(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    IPC::RequestParser rp{ctx};
    auto context{rp.PopRaw<Service::Time::Clock::SystemClockContext>()};

    auto res = SetUserSystemClockContext(context);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void ISystemSettingsServer::GetLockScreenFlag(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called, lock_screen_flag={}", m_system_settings.lock_screen_flag);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(m_system_settings.lock_screen_flag);
}

void ISystemSettingsServer::SetLockScreenFlag(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    m_system_settings.lock_screen_flag = rp.Pop<bool>();
    SetSaveNeeded();

    LOG_INFO(Service_SET, "called, lock_screen_flag={}", m_system_settings.lock_screen_flag);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISystemSettingsServer::GetAccountSettings(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushRaw(m_system_settings.account_settings);
}

void ISystemSettingsServer::SetAccountSettings(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    m_system_settings.account_settings = rp.PopRaw<AccountSettings>();
    SetSaveNeeded();

    LOG_INFO(Service_SET, "called, account_settings_flags={}",
             m_system_settings.account_settings.flags);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISystemSettingsServer::GetEulaVersions(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called, elements={}", m_system_settings.eula_version_count);

    ctx.WriteBuffer(m_system_settings.eula_versions);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(m_system_settings.eula_version_count);
}

void ISystemSettingsServer::SetEulaVersions(HLERequestContext& ctx) {
    const auto elements = ctx.GetReadBufferNumElements<EulaVersion>();
    const auto buffer_data = ctx.ReadBuffer();

    LOG_INFO(Service_SET, "called, elements={}", elements);
    ASSERT(elements <= m_system_settings.eula_versions.size());

    m_system_settings.eula_version_count = static_cast<u32>(elements);
    std::memcpy(&m_system_settings.eula_versions, buffer_data.data(),
                sizeof(EulaVersion) * elements);
    SetSaveNeeded();

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISystemSettingsServer::GetColorSetId(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called, color_set=", m_system_settings.color_set_id);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(m_system_settings.color_set_id);
}

void ISystemSettingsServer::SetColorSetId(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    m_system_settings.color_set_id = rp.PopEnum<ColorSet>();
    SetSaveNeeded();

    LOG_DEBUG(Service_SET, "called, color_set={}", m_system_settings.color_set_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISystemSettingsServer::GetNotificationSettings(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called, flags={}, volume={}, head_time={}:{}, tailt_time={}:{}",
             m_system_settings.notification_settings.flags.raw,
             m_system_settings.notification_settings.volume,
             m_system_settings.notification_settings.start_time.hour,
             m_system_settings.notification_settings.start_time.minute,
             m_system_settings.notification_settings.stop_time.hour,
             m_system_settings.notification_settings.stop_time.minute);

    IPC::ResponseBuilder rb{ctx, 8};
    rb.Push(ResultSuccess);
    rb.PushRaw(m_system_settings.notification_settings);
}

void ISystemSettingsServer::SetNotificationSettings(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    m_system_settings.notification_settings = rp.PopRaw<NotificationSettings>();
    SetSaveNeeded();

    LOG_INFO(Service_SET, "called, flags={}, volume={}, head_time={}:{}, tailt_time={}:{}",
             m_system_settings.notification_settings.flags.raw,
             m_system_settings.notification_settings.volume,
             m_system_settings.notification_settings.start_time.hour,
             m_system_settings.notification_settings.start_time.minute,
             m_system_settings.notification_settings.stop_time.hour,
             m_system_settings.notification_settings.stop_time.minute);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISystemSettingsServer::GetAccountNotificationSettings(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called, elements={}",
             m_system_settings.account_notification_settings_count);

    ctx.WriteBuffer(m_system_settings.account_notification_settings);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(m_system_settings.account_notification_settings_count);
}

void ISystemSettingsServer::SetAccountNotificationSettings(HLERequestContext& ctx) {
    const auto elements = ctx.GetReadBufferNumElements<AccountNotificationSettings>();
    const auto buffer_data = ctx.ReadBuffer();

    LOG_INFO(Service_SET, "called, elements={}", elements);

    ASSERT(elements <= m_system_settings.account_notification_settings.size());

    m_system_settings.account_notification_settings_count = static_cast<u32>(elements);
    std::memcpy(&m_system_settings.account_notification_settings, buffer_data.data(),
                elements * sizeof(AccountNotificationSettings));
    SetSaveNeeded();

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

    // AM
    ret["hbloader"]["applet_heap_size"] = ToBytes(u64{0x0});
    ret["hbloader"]["applet_heap_reservation_size"] = ToBytes(u64{0x8600000});

    // Time
    ret["time"]["notify_time_to_fs_interval_seconds"] = ToBytes(s32{600});
    ret["time"]["standard_network_clock_sufficient_accuracy_minutes"] =
        ToBytes(s32{43200}); // 30 days
    ret["time"]["standard_steady_clock_rtc_update_interval_minutes"] = ToBytes(s32{5});
    ret["time"]["standard_steady_clock_test_offset_minutes"] = ToBytes(s32{0});
    ret["time"]["standard_user_clock_initial_year"] = ToBytes(s32{2023});

    // HID
    ret["hid_debug"]["enables_debugpad"] = ToBytes(bool{true});
    ret["hid_debug"]["manages_devices"] = ToBytes(bool{true});
    ret["hid_debug"]["manages_touch_ic_i2c"] = ToBytes(bool{true});
    ret["hid_debug"]["emulate_future_device"] = ToBytes(bool{false});
    ret["hid_debug"]["emulate_mcu_hardware_error"] = ToBytes(bool{false});
    ret["hid_debug"]["enables_rail"] = ToBytes(bool{true});
    ret["hid_debug"]["emulate_firmware_update_failure"] = ToBytes(bool{false});
    ret["hid_debug"]["failure_firmware_update"] = ToBytes(s32{0});
    ret["hid_debug"]["ble_disabled"] = ToBytes(bool{false});
    ret["hid_debug"]["dscale_disabled"] = ToBytes(bool{false});
    ret["hid_debug"]["force_handheld"] = ToBytes(bool{true});
    ret["hid_debug"]["disabled_features_per_id"] = std::vector<u8>(0xa8);
    ret["hid_debug"]["touch_firmware_auto_update_disabled"] = ToBytes(bool{false});

    // Settings
    ret["settings_debug"]["is_debug_mode_enabled"] = ToBytes(bool{false});

    return ret;
}

void ISystemSettingsServer::GetSettingsItemValueSize(HLERequestContext& ctx) {
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

void ISystemSettingsServer::GetSettingsItemValue(HLERequestContext& ctx) {
    // The category of the setting. This corresponds to the top-level keys of
    // system_settings.ini.
    const auto setting_category_buf{ctx.ReadBuffer(0)};
    const std::string setting_category{setting_category_buf.begin(), setting_category_buf.end()};

    // The name of the setting. This corresponds to the second-level keys of
    // system_settings.ini.
    const auto setting_name_buf{ctx.ReadBuffer(1)};
    const std::string setting_name{setting_name_buf.begin(), setting_name_buf.end()};

    std::vector<u8> value;
    auto response = GetSettingsItemValue(value, setting_category, setting_name);

    LOG_INFO(Service_SET, "called. category={}, name={} -- res=0x{:X}", setting_category,
             setting_name, response.raw);

    ctx.WriteBuffer(value.data(), value.size());

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(response);
}

void ISystemSettingsServer::GetTvSettings(HLERequestContext& ctx) {
    LOG_INFO(Service_SET,
             "called, flags={}, cmu_mode={}, contrast_ratio={}, hdmi_content_type={}, "
             "rgb_range={}, tv_gama={}, tv_resolution={}, tv_underscan={}",
             m_system_settings.tv_settings.flags.raw, m_system_settings.tv_settings.cmu_mode,
             m_system_settings.tv_settings.contrast_ratio,
             m_system_settings.tv_settings.hdmi_content_type,
             m_system_settings.tv_settings.rgb_range, m_system_settings.tv_settings.tv_gama,
             m_system_settings.tv_settings.tv_resolution,
             m_system_settings.tv_settings.tv_underscan);

    IPC::ResponseBuilder rb{ctx, 10};
    rb.Push(ResultSuccess);
    rb.PushRaw(m_system_settings.tv_settings);
}

void ISystemSettingsServer::SetTvSettings(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    m_system_settings.tv_settings = rp.PopRaw<TvSettings>();
    SetSaveNeeded();

    LOG_INFO(Service_SET,
             "called, flags={}, cmu_mode={}, contrast_ratio={}, hdmi_content_type={}, "
             "rgb_range={}, tv_gama={}, tv_resolution={}, tv_underscan={}",
             m_system_settings.tv_settings.flags.raw, m_system_settings.tv_settings.cmu_mode,
             m_system_settings.tv_settings.contrast_ratio,
             m_system_settings.tv_settings.hdmi_content_type,
             m_system_settings.tv_settings.rgb_range, m_system_settings.tv_settings.tv_gama,
             m_system_settings.tv_settings.tv_resolution,
             m_system_settings.tv_settings.tv_underscan);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISystemSettingsServer::GetDebugModeFlag(HLERequestContext& ctx) {
    bool is_debug_mode_enabled = false;
    GetSettingsItemValue<bool>(is_debug_mode_enabled, "settings_debug", "is_debug_mode_enabled");

    LOG_DEBUG(Service_SET, "called, is_debug_mode_enabled={}", is_debug_mode_enabled);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(is_debug_mode_enabled);
}

void ISystemSettingsServer::GetQuestFlag(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called, quest_flag={}", m_system_settings.quest_flag);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(m_system_settings.quest_flag);
}

void ISystemSettingsServer::GetDeviceTimeZoneLocationName(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    Service::Time::TimeZone::LocationName name{};
    auto res = GetDeviceTimeZoneLocationName(name);

    IPC::ResponseBuilder rb{ctx, 2 + sizeof(Service::Time::TimeZone::LocationName) / sizeof(u32)};
    rb.Push(res);
    rb.PushRaw<Service::Time::TimeZone::LocationName>(name);
}

void ISystemSettingsServer::SetDeviceTimeZoneLocationName(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    IPC::RequestParser rp{ctx};
    auto name{rp.PopRaw<Service::Time::TimeZone::LocationName>()};

    auto res = SetDeviceTimeZoneLocationName(name);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void ISystemSettingsServer::SetRegionCode(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    m_system_settings.region_code = rp.PopEnum<SystemRegionCode>();
    SetSaveNeeded();

    LOG_INFO(Service_SET, "called, region_code={}", m_system_settings.region_code);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISystemSettingsServer::GetNetworkSystemClockContext(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    Service::Time::Clock::SystemClockContext context{};
    auto res = GetNetworkSystemClockContext(context);

    IPC::ResponseBuilder rb{ctx,
                            2 + sizeof(Service::Time::Clock::SystemClockContext) / sizeof(u32)};
    rb.Push(res);
    rb.PushRaw(context);
}

void ISystemSettingsServer::SetNetworkSystemClockContext(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    IPC::RequestParser rp{ctx};
    auto context{rp.PopRaw<Service::Time::Clock::SystemClockContext>()};

    auto res = SetNetworkSystemClockContext(context);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void ISystemSettingsServer::IsUserSystemClockAutomaticCorrectionEnabled(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    bool enabled{};
    auto res = IsUserSystemClockAutomaticCorrectionEnabled(enabled);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(res);
    rb.PushRaw(enabled);
}

void ISystemSettingsServer::SetUserSystemClockAutomaticCorrectionEnabled(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    IPC::RequestParser rp{ctx};
    auto enabled{rp.Pop<bool>()};

    auto res = SetUserSystemClockAutomaticCorrectionEnabled(enabled);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void ISystemSettingsServer::GetPrimaryAlbumStorage(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called, primary_album_storage={}",
             m_system_settings.primary_album_storage);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(m_system_settings.primary_album_storage);
}

void ISystemSettingsServer::GetNfcEnableFlag(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called, nfc_enable_flag={}", m_system_settings.nfc_enable_flag);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u8>(m_system_settings.nfc_enable_flag);
}

void ISystemSettingsServer::SetNfcEnableFlag(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    m_system_settings.nfc_enable_flag = rp.Pop<bool>();
    SetSaveNeeded();

    LOG_INFO(Service_SET, "called, nfc_enable_flag={}", m_system_settings.nfc_enable_flag);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISystemSettingsServer::GetSleepSettings(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called, flags={}, handheld_sleep_plan={}, console_sleep_plan={}",
             m_system_settings.sleep_settings.flags.raw,
             m_system_settings.sleep_settings.handheld_sleep_plan,
             m_system_settings.sleep_settings.console_sleep_plan);

    IPC::ResponseBuilder rb{ctx, 5};
    rb.Push(ResultSuccess);
    rb.PushRaw(m_system_settings.sleep_settings);
}

void ISystemSettingsServer::SetSleepSettings(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    m_system_settings.sleep_settings = rp.PopRaw<SleepSettings>();
    SetSaveNeeded();

    LOG_INFO(Service_SET, "called, flags={}, handheld_sleep_plan={}, console_sleep_plan={}",
             m_system_settings.sleep_settings.flags.raw,
             m_system_settings.sleep_settings.handheld_sleep_plan,
             m_system_settings.sleep_settings.console_sleep_plan);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISystemSettingsServer::GetWirelessLanEnableFlag(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called, wireless_lan_enable_flag={}",
             m_system_settings.wireless_lan_enable_flag);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(m_system_settings.wireless_lan_enable_flag);
}

void ISystemSettingsServer::SetWirelessLanEnableFlag(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    m_system_settings.wireless_lan_enable_flag = rp.Pop<bool>();
    SetSaveNeeded();

    LOG_INFO(Service_SET, "called, wireless_lan_enable_flag={}",
             m_system_settings.wireless_lan_enable_flag);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISystemSettingsServer::GetInitialLaunchSettings(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called, flags={}, timestamp={}",
             m_system_settings.initial_launch_settings_packed.flags.raw,
             m_system_settings.initial_launch_settings_packed.timestamp.time_point);

    IPC::ResponseBuilder rb{ctx, 10};
    rb.Push(ResultSuccess);
    rb.PushRaw(m_system_settings.initial_launch_settings_packed);
}

void ISystemSettingsServer::SetInitialLaunchSettings(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    auto initial_launch_settings = rp.PopRaw<InitialLaunchSettings>();

    m_system_settings.initial_launch_settings_packed.flags = initial_launch_settings.flags;
    m_system_settings.initial_launch_settings_packed.timestamp = initial_launch_settings.timestamp;
    SetSaveNeeded();

    LOG_INFO(Service_SET, "called, flags={}, timestamp={}",
             m_system_settings.initial_launch_settings_packed.flags.raw,
             m_system_settings.initial_launch_settings_packed.timestamp.time_point);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISystemSettingsServer::GetDeviceNickName(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");

    ctx.WriteBuffer(::Settings::values.device_name.GetValue());

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISystemSettingsServer::SetDeviceNickName(HLERequestContext& ctx) {
    const std::string device_name = Common::StringFromBuffer(ctx.ReadBuffer());

    LOG_INFO(Service_SET, "called, device_name={}", device_name);

    ::Settings::values.device_name = device_name;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISystemSettingsServer::GetProductModel(HLERequestContext& ctx) {
    const u32 product_model = 1;

    LOG_WARNING(Service_SET, "(STUBBED) called, product_model={}", product_model);
    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(product_model);
}

void ISystemSettingsServer::GetBluetoothEnableFlag(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called, bluetooth_enable_flag={}",
             m_system_settings.bluetooth_enable_flag);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u8>(m_system_settings.bluetooth_enable_flag);
}

void ISystemSettingsServer::SetBluetoothEnableFlag(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    m_system_settings.bluetooth_enable_flag = rp.Pop<bool>();
    SetSaveNeeded();

    LOG_INFO(Service_SET, "called, bluetooth_enable_flag={}",
             m_system_settings.bluetooth_enable_flag);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISystemSettingsServer::GetMiiAuthorId(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called, author_id={}",
             m_system_settings.mii_author_id.FormattedString());

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(ResultSuccess);
    rb.PushRaw(m_system_settings.mii_author_id);
}

void ISystemSettingsServer::GetAutoUpdateEnableFlag(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called, auto_update_flag={}", m_system_settings.auto_update_enable_flag);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(m_system_settings.auto_update_enable_flag);
}

void ISystemSettingsServer::GetBatteryPercentageFlag(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called, battery_percentage_flag={}",
              m_system_settings.battery_percentage_flag);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(m_system_settings.battery_percentage_flag);
}

void ISystemSettingsServer::SetExternalSteadyClockInternalOffset(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called.");

    IPC::RequestParser rp{ctx};
    auto offset{rp.Pop<s64>()};

    auto res = SetExternalSteadyClockInternalOffset(offset);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void ISystemSettingsServer::GetExternalSteadyClockInternalOffset(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called.");

    s64 offset{};
    auto res = GetExternalSteadyClockInternalOffset(offset);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(res);
    rb.Push(offset);
}

void ISystemSettingsServer::GetErrorReportSharePermission(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called, error_report_share_permission={}",
             m_system_settings.error_report_share_permission);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(m_system_settings.error_report_share_permission);
}

void ISystemSettingsServer::GetAppletLaunchFlags(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called, applet_launch_flag={}", m_system_settings.applet_launch_flag);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(m_system_settings.applet_launch_flag);
}

void ISystemSettingsServer::SetAppletLaunchFlags(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    m_system_settings.applet_launch_flag = rp.Pop<u32>();
    SetSaveNeeded();

    LOG_INFO(Service_SET, "called, applet_launch_flag={}", m_system_settings.applet_launch_flag);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISystemSettingsServer::GetKeyboardLayout(HLERequestContext& ctx) {
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

void ISystemSettingsServer::GetDeviceTimeZoneLocationUpdatedTime(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    Service::Time::Clock::SteadyClockTimePoint time_point{};
    auto res = GetDeviceTimeZoneLocationUpdatedTime(time_point);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(res);
    rb.PushRaw<Service::Time::Clock::SteadyClockTimePoint>(time_point);
}

void ISystemSettingsServer::SetDeviceTimeZoneLocationUpdatedTime(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    IPC::RequestParser rp{ctx};
    auto time_point{rp.PopRaw<Service::Time::Clock::SteadyClockTimePoint>()};

    auto res = SetDeviceTimeZoneLocationUpdatedTime(time_point);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void ISystemSettingsServer::GetUserSystemClockAutomaticCorrectionUpdatedTime(
    HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    Service::Time::Clock::SteadyClockTimePoint time_point{};
    auto res = GetUserSystemClockAutomaticCorrectionUpdatedTime(time_point);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(res);
    rb.PushRaw<Service::Time::Clock::SteadyClockTimePoint>(time_point);
}

void ISystemSettingsServer::SetUserSystemClockAutomaticCorrectionUpdatedTime(
    HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    IPC::RequestParser rp{ctx};
    auto time_point{rp.PopRaw<Service::Time::Clock::SteadyClockTimePoint>()};

    auto res = SetUserSystemClockAutomaticCorrectionUpdatedTime(time_point);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void ISystemSettingsServer::GetChineseTraditionalInputMethod(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called, chinese_traditional_input_method={}",
             m_system_settings.chinese_traditional_input_method);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(m_system_settings.chinese_traditional_input_method);
}

void ISystemSettingsServer::GetHomeMenuScheme(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "(STUBBED) called");

    const HomeMenuScheme default_color = {
        .main = 0xFF323232,
        .back = 0xFF323232,
        .sub = 0xFFFFFFFF,
        .bezel = 0xFFFFFFFF,
        .extra = 0xFF000000,
    };

    IPC::ResponseBuilder rb{ctx, 2 + sizeof(HomeMenuScheme) / sizeof(u32)};
    rb.Push(ResultSuccess);
    rb.PushRaw(default_color);
}

void ISystemSettingsServer::GetHomeMenuSchemeModel(HLERequestContext& ctx) {
    LOG_WARNING(Service_SET, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(0);
}

void ISystemSettingsServer::GetFieldTestingFlag(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called, field_testing_flag={}", m_system_settings.field_testing_flag);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(m_system_settings.field_testing_flag);
}

void ISystemSettingsServer::SetupSettings() {
    auto system_dir =
        Common::FS::GetYuzuPath(Common::FS::YuzuPath::NANDDir) / "system/save/8000000000000050";
    if (!LoadSettingsFile(system_dir, []() { return DefaultSystemSettings(); })) {
        ASSERT(false);
    }

    auto private_dir =
        Common::FS::GetYuzuPath(Common::FS::YuzuPath::NANDDir) / "system/save/8000000000000052";
    if (!LoadSettingsFile(private_dir, []() { return DefaultPrivateSettings(); })) {
        ASSERT(false);
    }

    auto device_dir =
        Common::FS::GetYuzuPath(Common::FS::YuzuPath::NANDDir) / "system/save/8000000000000053";
    if (!LoadSettingsFile(device_dir, []() { return DefaultDeviceSettings(); })) {
        ASSERT(false);
    }

    auto appln_dir =
        Common::FS::GetYuzuPath(Common::FS::YuzuPath::NANDDir) / "system/save/8000000000000054";
    if (!LoadSettingsFile(appln_dir, []() { return DefaultApplnSettings(); })) {
        ASSERT(false);
    }
}

void ISystemSettingsServer::StoreSettings() {
    auto system_dir =
        Common::FS::GetYuzuPath(Common::FS::YuzuPath::NANDDir) / "system/save/8000000000000050";
    if (!StoreSettingsFile(system_dir, m_system_settings)) {
        LOG_ERROR(HW_GPU, "Failed to store System settings");
    }

    auto private_dir =
        Common::FS::GetYuzuPath(Common::FS::YuzuPath::NANDDir) / "system/save/8000000000000052";
    if (!StoreSettingsFile(private_dir, m_private_settings)) {
        LOG_ERROR(HW_GPU, "Failed to store Private settings");
    }

    auto device_dir =
        Common::FS::GetYuzuPath(Common::FS::YuzuPath::NANDDir) / "system/save/8000000000000053";
    if (!StoreSettingsFile(device_dir, m_device_settings)) {
        LOG_ERROR(HW_GPU, "Failed to store Device settings");
    }

    auto appln_dir =
        Common::FS::GetYuzuPath(Common::FS::YuzuPath::NANDDir) / "system/save/8000000000000054";
    if (!StoreSettingsFile(appln_dir, m_appln_settings)) {
        LOG_ERROR(HW_GPU, "Failed to store ApplLn settings");
    }
}

void ISystemSettingsServer::StoreSettingsThreadFunc(std::stop_token stop_token) {
    Common::SetCurrentThreadName("SettingsStore");

    while (Common::StoppableTimedWait(stop_token, std::chrono::minutes(1))) {
        std::scoped_lock l{m_save_needed_mutex};
        if (!std::exchange(m_save_needed, false)) {
            continue;
        }
        StoreSettings();
    }
}

void ISystemSettingsServer::SetSaveNeeded() {
    std::scoped_lock l{m_save_needed_mutex};
    m_save_needed = true;
}

Result ISystemSettingsServer::GetSettingsItemValue(std::vector<u8>& out_value,
                                                   const std::string& category,
                                                   const std::string& name) {
    auto settings{GetSettings()};
    R_UNLESS(settings.contains(category) && settings[category].contains(name), ResultUnknown);

    out_value = settings[category][name];
    R_SUCCEED();
}

Result ISystemSettingsServer::GetExternalSteadyClockSourceId(Common::UUID& out_id) {
    out_id = m_private_settings.external_clock_source_id;
    R_SUCCEED();
}

Result ISystemSettingsServer::SetExternalSteadyClockSourceId(Common::UUID id) {
    m_private_settings.external_clock_source_id = id;
    SetSaveNeeded();
    R_SUCCEED();
}

Result ISystemSettingsServer::GetUserSystemClockContext(
    Service::Time::Clock::SystemClockContext& out_context) {
    out_context = m_system_settings.user_system_clock_context;
    R_SUCCEED();
}

Result ISystemSettingsServer::SetUserSystemClockContext(
    Service::Time::Clock::SystemClockContext& context) {
    m_system_settings.user_system_clock_context = context;
    SetSaveNeeded();
    R_SUCCEED();
}

Result ISystemSettingsServer::GetDeviceTimeZoneLocationName(
    Service::Time::TimeZone::LocationName& out_name) {
    out_name = m_system_settings.device_time_zone_location_name;
    R_SUCCEED();
}

Result ISystemSettingsServer::SetDeviceTimeZoneLocationName(
    Service::Time::TimeZone::LocationName& name) {
    m_system_settings.device_time_zone_location_name = name;
    SetSaveNeeded();
    R_SUCCEED();
}

Result ISystemSettingsServer::GetNetworkSystemClockContext(
    Service::Time::Clock::SystemClockContext& out_context) {
    out_context = m_system_settings.network_system_clock_context;
    R_SUCCEED();
}

Result ISystemSettingsServer::SetNetworkSystemClockContext(
    Service::Time::Clock::SystemClockContext& context) {
    m_system_settings.network_system_clock_context = context;
    SetSaveNeeded();
    R_SUCCEED();
}

Result ISystemSettingsServer::IsUserSystemClockAutomaticCorrectionEnabled(bool& out_enabled) {
    out_enabled = m_system_settings.user_system_clock_automatic_correction_enabled;
    R_SUCCEED();
}

Result ISystemSettingsServer::SetUserSystemClockAutomaticCorrectionEnabled(bool enabled) {
    m_system_settings.user_system_clock_automatic_correction_enabled = enabled;
    SetSaveNeeded();
    R_SUCCEED();
}

Result ISystemSettingsServer::SetExternalSteadyClockInternalOffset(s64 offset) {
    m_private_settings.external_steady_clock_internal_offset = offset;
    SetSaveNeeded();
    R_SUCCEED();
}

Result ISystemSettingsServer::GetExternalSteadyClockInternalOffset(s64& out_offset) {
    out_offset = m_private_settings.external_steady_clock_internal_offset;
    R_SUCCEED();
}

Result ISystemSettingsServer::GetDeviceTimeZoneLocationUpdatedTime(
    Service::Time::Clock::SteadyClockTimePoint& out_time_point) {
    out_time_point = m_system_settings.device_time_zone_location_updated_time;
    R_SUCCEED();
}

Result ISystemSettingsServer::SetDeviceTimeZoneLocationUpdatedTime(
    Service::Time::Clock::SteadyClockTimePoint& time_point) {
    m_system_settings.device_time_zone_location_updated_time = time_point;
    SetSaveNeeded();
    R_SUCCEED();
}

Result ISystemSettingsServer::GetUserSystemClockAutomaticCorrectionUpdatedTime(
    Service::Time::Clock::SteadyClockTimePoint& out_time_point) {
    out_time_point = m_system_settings.user_system_clock_automatic_correction_updated_time_point;
    R_SUCCEED();
}

Result ISystemSettingsServer::SetUserSystemClockAutomaticCorrectionUpdatedTime(
    Service::Time::Clock::SteadyClockTimePoint out_time_point) {
    m_system_settings.user_system_clock_automatic_correction_updated_time_point = out_time_point;
    SetSaveNeeded();
    R_SUCCEED();
}

} // namespace Service::Set
