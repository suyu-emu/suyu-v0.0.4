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
#include "core/hle/service/service.h"
#include "core/hle/service/set/appln_settings.h"
#include "core/hle/service/set/device_settings.h"
#include "core/hle/service/set/private_settings.h"
#include "core/hle/service/set/system_settings.h"
#include "core/hle/service/time/clock_types.h"
#include "core/hle/service/time/time_zone_types.h"

namespace Core {
class System;
}

namespace Service::Set {
enum class GetFirmwareVersionType {
    Version1,
    Version2,
};

struct FirmwareVersionFormat {
    u8 major;
    u8 minor;
    u8 micro;
    INSERT_PADDING_BYTES(1);
    u8 revision_major;
    u8 revision_minor;
    INSERT_PADDING_BYTES(2);
    std::array<char, 0x20> platform;
    std::array<u8, 0x40> version_hash;
    std::array<char, 0x18> display_version;
    std::array<char, 0x80> display_title;
};
static_assert(sizeof(FirmwareVersionFormat) == 0x100, "FirmwareVersionFormat is an invalid size");

Result GetFirmwareVersionImpl(FirmwareVersionFormat& out_firmware, Core::System& system,
                              GetFirmwareVersionType type);

class SET_SYS final : public ServiceFramework<SET_SYS> {
public:
    explicit SET_SYS(Core::System& system_);
    ~SET_SYS() override;

    Result GetSettingsItemValue(std::vector<u8>& out_value, const std::string& category,
                                const std::string& name);

    Result GetExternalSteadyClockSourceId(Common::UUID& out_id);
    Result SetExternalSteadyClockSourceId(Common::UUID id);
    Result GetUserSystemClockContext(Service::Time::Clock::SystemClockContext& out_context);
    Result SetUserSystemClockContext(Service::Time::Clock::SystemClockContext& context);
    Result GetDeviceTimeZoneLocationName(Service::Time::TimeZone::LocationName& out_name);
    Result SetDeviceTimeZoneLocationName(Service::Time::TimeZone::LocationName& name);
    Result GetNetworkSystemClockContext(Service::Time::Clock::SystemClockContext& out_context);
    Result SetNetworkSystemClockContext(Service::Time::Clock::SystemClockContext& context);
    Result IsUserSystemClockAutomaticCorrectionEnabled(bool& out_enabled);
    Result SetUserSystemClockAutomaticCorrectionEnabled(bool enabled);
    Result SetExternalSteadyClockInternalOffset(s64 offset);
    Result GetExternalSteadyClockInternalOffset(s64& out_offset);
    Result GetDeviceTimeZoneLocationUpdatedTime(
        Service::Time::Clock::SteadyClockTimePoint& out_time_point);
    Result SetDeviceTimeZoneLocationUpdatedTime(
        Service::Time::Clock::SteadyClockTimePoint& time_point);
    Result GetUserSystemClockAutomaticCorrectionUpdatedTime(
        Service::Time::Clock::SteadyClockTimePoint& out_time_point);
    Result SetUserSystemClockAutomaticCorrectionUpdatedTime(
        Service::Time::Clock::SteadyClockTimePoint time_point);

private:
    void SetLanguageCode(HLERequestContext& ctx);
    void GetFirmwareVersion(HLERequestContext& ctx);
    void GetFirmwareVersion2(HLERequestContext& ctx);
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
    void GetSleepSettings(HLERequestContext& ctx);
    void SetSleepSettings(HLERequestContext& ctx);
    void GetInitialLaunchSettings(HLERequestContext& ctx);
    void SetInitialLaunchSettings(HLERequestContext& ctx);
    void GetDeviceNickName(HLERequestContext& ctx);
    void SetDeviceNickName(HLERequestContext& ctx);
    void GetProductModel(HLERequestContext& ctx);
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
    std::jthread m_save_thread;
    std::mutex m_save_needed_mutex;
    bool m_save_needed{false};
};

} // namespace Service::Set
