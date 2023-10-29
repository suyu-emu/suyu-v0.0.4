// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

#include "common/polyfill_thread.h"
#include "common/uuid.h"
#include "core/hle/result.h"
#include "core/hle/service/psc/time/common.h"
#include "core/hle/service/service.h"
#include "core/hle/service/set/setting_formats/appln_settings.h"
#include "core/hle/service/set/setting_formats/device_settings.h"
#include "core/hle/service/set/setting_formats/private_settings.h"
#include "core/hle/service/set/setting_formats/system_settings.h"
#include "core/hle/service/set/settings_types.h"

namespace Core {
class System;
}

namespace Service::Set {

Result GetFirmwareVersionImpl(FirmwareVersionFormat& out_firmware, Core::System& system,
                              GetFirmwareVersionType type);

class ISystemSettingsServer final : public ServiceFramework<ISystemSettingsServer> {
public:
    explicit ISystemSettingsServer(Core::System& system_);
    ~ISystemSettingsServer() override;

    Result GetSettingsItemValue(std::vector<u8>& out_value, const std::string& category,
                                const std::string& name);

    template <typename T>
    Result GetSettingsItemValue(T& value, const std::string& category, const std::string& name) {
        std::vector<u8> data;
        const auto result = GetSettingsItemValue(data, category, name);
        if (result.IsError()) {
            return result;
        }
        ASSERT(data.size() >= sizeof(T));
        std::memcpy(&value, data.data(), sizeof(T));
        return result;
    }

    Result GetExternalSteadyClockSourceId(Common::UUID& out_id);
    Result SetExternalSteadyClockSourceId(Common::UUID id);
    Result GetUserSystemClockContext(Service::PSC::Time::SystemClockContext& out_context);
    Result SetUserSystemClockContext(Service::PSC::Time::SystemClockContext& context);
    Result GetDeviceTimeZoneLocationName(Service::PSC::Time::LocationName& out_name);
    Result SetDeviceTimeZoneLocationName(Service::PSC::Time::LocationName& name);
    Result GetNetworkSystemClockContext(Service::PSC::Time::SystemClockContext& out_context);
    Result SetNetworkSystemClockContext(Service::PSC::Time::SystemClockContext& context);
    Result IsUserSystemClockAutomaticCorrectionEnabled(bool& out_enabled);
    Result SetUserSystemClockAutomaticCorrectionEnabled(bool enabled);
    Result SetExternalSteadyClockInternalOffset(s64 offset);
    Result GetExternalSteadyClockInternalOffset(s64& out_offset);
    Result GetDeviceTimeZoneLocationUpdatedTime(
        Service::PSC::Time::SteadyClockTimePoint& out_time_point);
    Result SetDeviceTimeZoneLocationUpdatedTime(
        Service::PSC::Time::SteadyClockTimePoint& time_point);
    Result GetUserSystemClockAutomaticCorrectionUpdatedTime(
        Service::PSC::Time::SteadyClockTimePoint& out_time_point);
    Result SetUserSystemClockAutomaticCorrectionUpdatedTime(
        Service::PSC::Time::SteadyClockTimePoint time_point);

private:
    void SetLanguageCode(HLERequestContext& ctx);
    void GetFirmwareVersion(HLERequestContext& ctx);
    void GetFirmwareVersion2(HLERequestContext& ctx);
    void GetLockScreenFlag(HLERequestContext& ctx);
    void SetLockScreenFlag(HLERequestContext& ctx);
    void GetExternalSteadyClockSourceId(HLERequestContext& ctx);
    void SetExternalSteadyClockSourceId(HLERequestContext& ctx);
    void GetUserSystemClockContext(HLERequestContext& ctx);
    void SetUserSystemClockContext(HLERequestContext& ctx);
    void GetAccountSettings(HLERequestContext& ctx);
    void SetAccountSettings(HLERequestContext& ctx);
    void GetEulaVersions(HLERequestContext& ctx);
    void SetEulaVersions(HLERequestContext& ctx);
    void GetColorSetId(HLERequestContext& ctx);
    void SetColorSetId(HLERequestContext& ctx);
    void GetNotificationSettings(HLERequestContext& ctx);
    void SetNotificationSettings(HLERequestContext& ctx);
    void GetAccountNotificationSettings(HLERequestContext& ctx);
    void SetAccountNotificationSettings(HLERequestContext& ctx);
    void GetSettingsItemValueSize(HLERequestContext& ctx);
    void GetSettingsItemValue(HLERequestContext& ctx);
    void GetTvSettings(HLERequestContext& ctx);
    void SetTvSettings(HLERequestContext& ctx);
    void GetDebugModeFlag(HLERequestContext& ctx);
    void GetQuestFlag(HLERequestContext& ctx);
    void GetDeviceTimeZoneLocationName(HLERequestContext& ctx);
    void SetDeviceTimeZoneLocationName(HLERequestContext& ctx);
    void SetRegionCode(HLERequestContext& ctx);
    void GetNetworkSystemClockContext(HLERequestContext& ctx);
    void SetNetworkSystemClockContext(HLERequestContext& ctx);
    void IsUserSystemClockAutomaticCorrectionEnabled(HLERequestContext& ctx);
    void SetUserSystemClockAutomaticCorrectionEnabled(HLERequestContext& ctx);
    void GetPrimaryAlbumStorage(HLERequestContext& ctx);
    void GetNfcEnableFlag(HLERequestContext& ctx);
    void SetNfcEnableFlag(HLERequestContext& ctx);
    void GetSleepSettings(HLERequestContext& ctx);
    void SetSleepSettings(HLERequestContext& ctx);
    void GetWirelessLanEnableFlag(HLERequestContext& ctx);
    void SetWirelessLanEnableFlag(HLERequestContext& ctx);
    void GetInitialLaunchSettings(HLERequestContext& ctx);
    void SetInitialLaunchSettings(HLERequestContext& ctx);
    void GetDeviceNickName(HLERequestContext& ctx);
    void SetDeviceNickName(HLERequestContext& ctx);
    void GetProductModel(HLERequestContext& ctx);
    void GetBluetoothEnableFlag(HLERequestContext& ctx);
    void SetBluetoothEnableFlag(HLERequestContext& ctx);
    void GetMiiAuthorId(HLERequestContext& ctx);
    void GetAutoUpdateEnableFlag(HLERequestContext& ctx);
    void GetBatteryPercentageFlag(HLERequestContext& ctx);
    void SetExternalSteadyClockInternalOffset(HLERequestContext& ctx);
    void GetExternalSteadyClockInternalOffset(HLERequestContext& ctx);
    void GetErrorReportSharePermission(HLERequestContext& ctx);
    void GetAppletLaunchFlags(HLERequestContext& ctx);
    void SetAppletLaunchFlags(HLERequestContext& ctx);
    void GetKeyboardLayout(HLERequestContext& ctx);
    void GetDeviceTimeZoneLocationUpdatedTime(HLERequestContext& ctx);
    void SetDeviceTimeZoneLocationUpdatedTime(HLERequestContext& ctx);
    void GetUserSystemClockAutomaticCorrectionUpdatedTime(HLERequestContext& ctx);
    void SetUserSystemClockAutomaticCorrectionUpdatedTime(HLERequestContext& ctx);
    void GetChineseTraditionalInputMethod(HLERequestContext& ctx);
    void GetHomeMenuScheme(HLERequestContext& ctx);
    void GetHomeMenuSchemeModel(HLERequestContext& ctx);
    void GetFieldTestingFlag(HLERequestContext& ctx);

    bool LoadSettingsFile(std::filesystem::path& path, auto&& default_func);
    bool StoreSettingsFile(std::filesystem::path& path, auto& settings);
    void SetupSettings();
    void StoreSettings();
    void StoreSettingsThreadFunc(std::stop_token stop_token);
    void SetSaveNeeded();

    Core::System& m_system;
    SystemSettings m_system_settings{};
    PrivateSettings m_private_settings{};
    DeviceSettings m_device_settings{};
    ApplnSettings m_appln_settings{};
    std::mutex m_save_needed_mutex;
    std::jthread m_save_thread;
    bool m_save_needed{false};
};

} // namespace Service::Set
