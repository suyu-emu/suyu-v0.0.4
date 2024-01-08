// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "core/hle/service/set/private_settings.h"
#include "core/hle/service/time/clock_types.h"

namespace Service::Set {

/// This is "nn::settings::LanguageCode", which is a NUL-terminated string stored in a u64.
enum class LanguageCode : u64 {
    JA = 0x000000000000616A,
    EN_US = 0x00000053552D6E65,
    FR = 0x0000000000007266,
    DE = 0x0000000000006564,
    IT = 0x0000000000007469,
    ES = 0x0000000000007365,
    ZH_CN = 0x0000004E432D687A,
    KO = 0x0000000000006F6B,
    NL = 0x0000000000006C6E,
    PT = 0x0000000000007470,
    RU = 0x0000000000007572,
    ZH_TW = 0x00000057542D687A,
    EN_GB = 0x00000042472D6E65,
    FR_CA = 0x00000041432D7266,
    ES_419 = 0x00003931342D7365,
    ZH_HANS = 0x00736E61482D687A,
    ZH_HANT = 0x00746E61482D687A,
    PT_BR = 0x00000052422D7470,
};

/// This is nn::settings::system::ErrorReportSharePermission
enum class ErrorReportSharePermission : u32 {
    NotConfirmed,
    Granted,
    Denied,
};

/// This is nn::settings::system::ChineseTraditionalInputMethod
enum class ChineseTraditionalInputMethod : u32 {
    Unknown0 = 0,
    Unknown1 = 1,
    Unknown2 = 2,
};

/// This is nn::settings::system::HomeMenuScheme
struct HomeMenuScheme {
    u32 main;
    u32 back;
    u32 sub;
    u32 bezel;
    u32 extra;
};
static_assert(sizeof(HomeMenuScheme) == 0x14, "HomeMenuScheme is incorrect size");

/// Indicates the current theme set by the system settings
enum class ColorSet : u32 {
    BasicWhite = 0,
    BasicBlack = 1,
};

/// Indicates the current console is a retail or kiosk unit
enum class QuestFlag : u8 {
    Retail = 0,
    Kiosk = 1,
};

/// This is nn::settings::system::RegionCode
enum class RegionCode : u32 {
    Japan,
    Usa,
    Europe,
    Australia,
    HongKongTaiwanKorea,
    China,
};

/// This is nn::settings::system::AccountSettings
struct AccountSettings {
    u32 flags;
};
static_assert(sizeof(AccountSettings) == 4, "AccountSettings is an invalid size");

/// This is nn::settings::system::NotificationVolume
enum class NotificationVolume : u32 {
    Mute,
    Low,
    High,
};

/// This is nn::settings::system::NotificationFlag
struct NotificationFlag {
    union {
        u32 raw{};

        BitField<0, 1, u32> RingtoneFlag;
        BitField<1, 1, u32> DownloadCompletionFlag;
        BitField<8, 1, u32> EnablesNews;
        BitField<9, 1, u32> IncomingLampFlag;
    };
};
static_assert(sizeof(NotificationFlag) == 4, "NotificationFlag is an invalid size");

/// This is nn::settings::system::NotificationTime
struct NotificationTime {
    u32 hour;
    u32 minute;
};
static_assert(sizeof(NotificationTime) == 0x8, "NotificationTime is an invalid size");

/// This is nn::settings::system::NotificationSettings
struct NotificationSettings {
    NotificationFlag flags;
    NotificationVolume volume;
    NotificationTime start_time;
    NotificationTime stop_time;
};
static_assert(sizeof(NotificationSettings) == 0x18, "NotificationSettings is an invalid size");

/// This is nn::settings::system::AccountNotificationFlag
struct AccountNotificationFlag {
    union {
        u32 raw{};

        BitField<0, 1, u32> FriendOnlineFlag;
        BitField<1, 1, u32> FriendRequestFlag;
        BitField<8, 1, u32> CoralInvitationFlag;
    };
};
static_assert(sizeof(AccountNotificationFlag) == 4, "AccountNotificationFlag is an invalid size");

/// This is nn::settings::system::FriendPresenceOverlayPermission
enum class FriendPresenceOverlayPermission : u8 {
    NotConfirmed,
    NoDisplay,
    FavoriteFriends,
    Friends,
};

/// This is nn::settings::system::AccountNotificationSettings
struct AccountNotificationSettings {
    Common::UUID uid;
    AccountNotificationFlag flags;
    FriendPresenceOverlayPermission friend_presence_permission;
    FriendPresenceOverlayPermission friend_invitation_permission;
    INSERT_PADDING_BYTES(0x2);
};
static_assert(sizeof(AccountNotificationSettings) == 0x18,
              "AccountNotificationSettings is an invalid size");

/// This is nn::settings::system::TvFlag
struct TvFlag {
    union {
        u32 raw{};

        BitField<0, 1, u32> Allows4k;
        BitField<1, 1, u32> Allows3d;
        BitField<2, 1, u32> AllowsCec;
        BitField<3, 1, u32> PreventsScreenBurnIn;
    };
};
static_assert(sizeof(TvFlag) == 4, "TvFlag is an invalid size");

/// This is nn::settings::system::TvResolution
enum class TvResolution : u32 {
    Auto,
    Resolution1080p,
    Resolution720p,
    Resolution480p,
};

/// This is nn::settings::system::HdmiContentType
enum class HdmiContentType : u32 {
    None,
    Graphics,
    Cinema,
    Photo,
    Game,
};

/// This is nn::settings::system::RgbRange
enum class RgbRange : u32 {
    Auto,
    Full,
    Limited,
};

/// This is nn::settings::system::CmuMode
enum class CmuMode : u32 {
    None,
    ColorInvert,
    HighContrast,
    GrayScale,
};

/// This is nn::settings::system::TvSettings
struct TvSettings {
    TvFlag flags;
    TvResolution tv_resolution;
    HdmiContentType hdmi_content_type;
    RgbRange rgb_range;
    CmuMode cmu_mode;
    u32 tv_underscan;
    f32 tv_gama;
    f32 contrast_ratio;
};
static_assert(sizeof(TvSettings) == 0x20, "TvSettings is an invalid size");

/// This is nn::settings::system::PrimaryAlbumStorage
enum class PrimaryAlbumStorage : u32 {
    Nand,
    SdCard,
};

/// This is nn::settings::system::HandheldSleepPlan
enum class HandheldSleepPlan : u32 {
    Sleep1Min,
    Sleep3Min,
    Sleep5Min,
    Sleep10Min,
    Sleep30Min,
    Never,
};

/// This is nn::settings::system::ConsoleSleepPlan
enum class ConsoleSleepPlan : u32 {
    Sleep1Hour,
    Sleep2Hour,
    Sleep3Hour,
    Sleep6Hour,
    Sleep12Hour,
    Never,
};

/// This is nn::settings::system::SleepFlag
struct SleepFlag {
    union {
        u32 raw{};

        BitField<0, 1, u32> SleepsWhilePlayingMedia;
        BitField<1, 1, u32> WakesAtPowerStateChange;
    };
};
static_assert(sizeof(SleepFlag) == 4, "TvFlag is an invalid size");

/// This is nn::settings::system::SleepSettings
struct SleepSettings {
    SleepFlag flags;
    HandheldSleepPlan handheld_sleep_plan;
    ConsoleSleepPlan console_sleep_plan;
};
static_assert(sizeof(SleepSettings) == 0xc, "SleepSettings is incorrect size");

/// This is nn::settings::system::EulaVersionClockType
enum class EulaVersionClockType : u32 {
    NetworkSystemClock,
    SteadyClock,
};

/// This is nn::settings::system::EulaVersion
struct EulaVersion {
    u32 version;
    RegionCode region_code;
    EulaVersionClockType clock_type;
    INSERT_PADDING_BYTES(0x4);
    s64 posix_time;
    Time::Clock::SteadyClockTimePoint timestamp;
};
static_assert(sizeof(EulaVersion) == 0x30, "EulaVersion is incorrect size");

struct SystemSettings {
    // 0/unwritten (1.0.0), 0x20000 (2.0.0), 0x30000 (3.0.0-3.0.1), 0x40001 (4.0.0-4.1.0), 0x50000
    // (5.0.0-5.1.0), 0x60000 (6.0.0-6.2.0), 0x70000 (7.0.0), 0x80000 (8.0.0-8.1.1), 0x90000
    // (9.0.0-10.0.4), 0x100100 (10.1.0+), 0x120000 (12.0.0-12.1.0), 0x130000 (13.0.0-13.2.1),
    // 0x140000 (14.0.0+)
    u32 version;
    // 0/unwritten (1.0.0), 1 (6.0.0-8.1.0), 2 (8.1.1), 7 (9.0.0+).
    // if (flags & 2), defaults are written for AnalogStickUserCalibration
    u32 flags;

    std::array<u8, 0x8> reserved_00008;

    // nn::settings::LanguageCode
    LanguageCode language_code;

    std::array<u8, 0x38> reserved_00018;

    // nn::settings::system::NetworkSettings
    u32 network_setting_count;
    bool wireless_lan_enable_flag;
    std::array<u8, 0x3> pad_00055;

    std::array<u8, 0x8> reserved_00058;

    // nn::settings::system::NetworkSettings
    std::array<std::array<u8, 0x400>, 32> network_settings_1B0;

    // nn::settings::system::BluetoothDevicesSettings
    std::array<u8, 0x4> bluetooth_device_settings_count;
    bool bluetooth_enable_flag;
    std::array<u8, 0x3> pad_08065;
    bool bluetooth_afh_enable_flag;
    std::array<u8, 0x3> pad_08069;
    bool bluetooth_boost_enable_flag;
    std::array<u8, 0x3> pad_0806D;
    std::array<std::array<u8, 0x200>, 10> bluetooth_device_settings_first_10;

    s32 ldn_channel;

    std::array<u8, 0x3C> reserved_09474;

    // nn::util::Uuid MiiAuthorId
    std::array<u8, 0x10> mii_author_id;

    std::array<u8, 0x30> reserved_094C0;

    // nn::settings::system::NxControllerSettings
    u32 nx_controller_settings_count;

    std::array<u8, 0xC> reserved_094F4;

    // nn::settings::system::NxControllerSettings,
    // nn::settings::system::NxControllerLegacySettings on 13.0.0+
    std::array<std::array<u8, 0x40>, 10> nx_controller_legacy_settings;

    std::array<u8, 0x170> reserved_09780;

    bool external_rtc_reset_flag;
    std::array<u8, 0x3> pad_098F1;

    std::array<u8, 0x3C> reserved_098F4;

    s32 push_notification_activity_mode_on_sleep;

    std::array<u8, 0x3C> reserved_09934;

    // nn::settings::system::ErrorReportSharePermission
    ErrorReportSharePermission error_report_share_permission;

    std::array<u8, 0x3C> reserved_09974;

    // nn::settings::KeyboardLayout
    std::array<u8, 0x4> keyboard_layout;

    std::array<u8, 0x3C> reserved_099B4;

    bool web_inspector_flag;
    std::array<u8, 0x3> pad_099F1;

    // nn::settings::system::AllowedSslHost
    u32 allowed_ssl_host_count;

    bool memory_usage_rate_flag;
    std::array<u8, 0x3> pad_099F9;

    std::array<u8, 0x34> reserved_099FC;

    // nn::settings::system::HostFsMountPoint
    std::array<u8, 0x100> host_fs_mount_point;

    // nn::settings::system::AllowedSslHost
    std::array<std::array<u8, 0x100>, 8> allowed_ssl_hosts;

    std::array<u8, 0x6C0> reserved_0A330;

    // nn::settings::system::BlePairingSettings
    u32 ble_pairing_settings_count;
    std::array<u8, 0xC> reserved_0A9F4;
    std::array<std::array<u8, 0x80>, 10> ble_pairing_settings;

    // nn::settings::system::AccountOnlineStorageSettings
    u32 account_online_storage_settings_count;
    std::array<u8, 0xC> reserved_0AF04;
    std::array<std::array<u8, 0x40>, 8> account_online_storage_settings;

    bool pctl_ready_flag;
    std::array<u8, 0x3> pad_0B111;

    std::array<u8, 0x3C> reserved_0B114;

    // nn::settings::system::ThemeId
    std::array<u8, 0x80> theme_id_type0;
    std::array<u8, 0x80> theme_id_type1;

    std::array<u8, 0x100> reserved_0B250;

    // nn::settings::ChineseTraditionalInputMethod
    ChineseTraditionalInputMethod chinese_traditional_input_method;

    std::array<u8, 0x3C> reserved_0B354;

    bool zoom_flag;
    std::array<u8, 0x3> pad_0B391;

    std::array<u8, 0x3C> reserved_0B394;

    // nn::settings::system::ButtonConfigRegisteredSettings
    u32 button_config_registered_settings_count;
    std::array<u8, 0xC> reserved_0B3D4;

    // nn::settings::system::ButtonConfigSettings
    u32 button_config_settings_count;
    std::array<u8, 0x4> reserved_0B3E4;
    std::array<std::array<u8, 0x5A8>, 5> button_config_settings;
    std::array<u8, 0x13B0> reserved_0D030;
    u32 button_config_settings_embedded_count;
    std::array<u8, 0x4> reserved_0E3E4;
    std::array<std::array<u8, 0x5A8>, 5> button_config_settings_embedded;
    std::array<u8, 0x13B0> reserved_10030;
    u32 button_config_settings_left_count;
    std::array<u8, 0x4> reserved_113E4;
    std::array<std::array<u8, 0x5A8>, 5> button_config_settings_left;
    std::array<u8, 0x13B0> reserved_13030;
    u32 button_config_settings_right_count;
    std::array<u8, 0x4> reserved_143E4;
    std::array<std::array<u8, 0x5A8>, 5> button_config_settings_right;
    std::array<u8, 0x73B0> reserved_16030;
    // nn::settings::system::ButtonConfigRegisteredSettings
    std::array<u8, 0x5C8> button_config_registered_settings_embedded;
    std::array<std::array<u8, 0x5C8>, 10> button_config_registered_settings;

    std::array<u8, 0x7FF8> reserved_21378;

    // nn::settings::system::ConsoleSixAxisSensorAccelerationBias
    std::array<u8, 0xC> console_six_axis_sensor_acceleration_bias;
    // nn::settings::system::ConsoleSixAxisSensorAngularVelocityBias
    std::array<u8, 0xC> console_six_axis_sensor_angular_velocity_bias;
    // nn::settings::system::ConsoleSixAxisSensorAccelerationGain
    std::array<u8, 0x24> console_six_axis_sensor_acceleration_gain;
    // nn::settings::system::ConsoleSixAxisSensorAngularVelocityGain
    std::array<u8, 0x24> console_six_axis_sensor_angular_velocity_gain;
    // nn::settings::system::ConsoleSixAxisSensorAngularVelocityTimeBias
    std::array<u8, 0xC> console_six_axis_sensor_angular_velocity_time_bias;
    // nn::settings::system::ConsoleSixAxisSensorAngularAcceleration
    std::array<u8, 0x24> console_six_axis_sensor_angular_velocity_acceleration;

    std::array<u8, 0x70> reserved_29400;

    bool lock_screen_flag;
    std::array<u8, 0x3> pad_29471;

    std::array<u8, 0x4> reserved_249274;

    ColorSet color_set_id;

    QuestFlag quest_flag;

    // nn::settings::system::RegionCode
    RegionCode region_code;

    // Different to nn::settings::system::InitialLaunchSettings?
    InitialLaunchSettingsPacked initial_launch_settings_packed;

    bool battery_percentage_flag;
    std::array<u8, 0x3> pad_294A1;

    // BitFlagSet<32, nn::settings::system::AppletLaunchFlag>
    u32 applet_launch_flag;

    // nn::settings::system::ThemeSettings
    std::array<u8, 0x8> theme_settings;
    // nn::fssystem::ArchiveMacKey
    std::array<u8, 0x10> theme_key;

    bool field_testing_flag;
    std::array<u8, 0x3> pad_294C1;

    s32 panel_crc_mode;

    std::array<u8, 0x28> reserved_294C8;

    // nn::settings::system::BacklightSettings
    std::array<u8, 0x2C> backlight_settings_mixed_up;

    std::array<u8, 0x64> reserved_2951C;

    // nn::time::SystemClockContext
    Service::Time::Clock::SystemClockContext user_system_clock_context;
    Service::Time::Clock::SystemClockContext network_system_clock_context;
    bool user_system_clock_automatic_correction_enabled;
    std::array<u8, 0x3> pad_295C1;
    std::array<u8, 0x4> reserved_295C4;
    // nn::time::SteadyClockTimePoint
    Service::Time::Clock::SteadyClockTimePoint
        user_system_clock_automatic_correction_updated_time_point;

    std::array<u8, 0x10> reserved_295E0;

    // nn::settings::system::AccountSettings
    AccountSettings account_settings;

    std::array<u8, 0xFC> reserved_295F4;

    // nn::settings::system::AudioVolume
    std::array<u8, 0x8> audio_volume_type0;
    std::array<u8, 0x8> audio_volume_type1;
    // nn::settings::system::AudioOutputMode
    s32 audio_output_mode_type0;
    s32 audio_output_mode_type1;
    s32 audio_output_mode_type2;
    bool force_mute_on_headphone_removed;
    std::array<u8, 0x3> pad_2970D;
    s32 headphone_volume_warning_count;
    bool heaphone_volume_update_flag;
    std::array<u8, 0x3> pad_29715;
    // nn::settings::system::AudioVolume
    std::array<u8, 0x8> audio_volume_type2;
    // nn::settings::system::AudioOutputMode
    s32 audio_output_mode_type3;
    s32 audio_output_mode_type4;
    bool hearing_protection_safeguard_flag;
    std::array<u8, 0x3> pad_29729;
    std::array<u8, 0x4> reserved_2972C;
    s64 hearing_protection_safeguard_remaining_time;
    std::array<u8, 0x38> reserved_29738;

    bool console_information_upload_flag;
    std::array<u8, 0x3> pad_29771;

    std::array<u8, 0x3C> reserved_29774;

    bool automatic_application_download_flag;
    std::array<u8, 0x3> pad_297B1;

    std::array<u8, 0x4> reserved_297B4;

    // nn::settings::system::NotificationSettings
    NotificationSettings notification_settings;

    std::array<u8, 0x60> reserved_297D0;

    // nn::settings::system::AccountNotificationSettings
    u32 account_notification_settings_count;
    std::array<u8, 0xC> reserved_29834;
    std::array<AccountNotificationSettings, 8> account_notification_settings;

    std::array<u8, 0x140> reserved_29900;

    f32 vibration_master_volume;

    bool usb_full_key_enable_flag;
    std::array<u8, 0x3> pad_29A45;

    // nn::settings::system::AnalogStickUserCalibration
    std::array<u8, 0x10> analog_stick_user_calibration_left;
    std::array<u8, 0x10> analog_stick_user_calibration_right;

    // nn::settings::system::TouchScreenMode
    s32 touch_screen_mode;

    std::array<u8, 0x14> reserved_29A6C;

    // nn::settings::system::TvSettings
    TvSettings tv_settings;

    // nn::settings::system::Edid
    std::array<u8, 0x100> edid;

    std::array<u8, 0x2E0> reserved_29BA0;

    // nn::settings::system::DataDeletionSettings
    std::array<u8, 0x8> data_deletion_settings;

    std::array<u8, 0x38> reserved_29E88;

    // nn::ncm::ProgramId
    std::array<u8, 0x8> initial_system_applet_program_id;
    std::array<u8, 0x8> overlay_disp_program_id;

    std::array<u8, 0x4> reserved_29ED0;

    bool requires_run_repair_time_reviser;

    std::array<u8, 0x6B> reserved_29ED5;

    // nn::time::LocationName
    Service::Time::TimeZone::LocationName device_time_zone_location_name;
    std::array<u8, 0x4> reserved_29F64;
    // nn::time::SteadyClockTimePoint
    Service::Time::Clock::SteadyClockTimePoint device_time_zone_location_updated_time;

    std::array<u8, 0xC0> reserved_29F80;

    // nn::settings::system::PrimaryAlbumStorage
    PrimaryAlbumStorage primary_album_storage;

    std::array<u8, 0x3C> reserved_2A044;

    bool usb_30_enable_flag;
    std::array<u8, 0x3> pad_2A081;
    bool usb_30_host_enable_flag;
    std::array<u8, 0x3> pad_2A085;
    bool usb_30_device_enable_flag;
    std::array<u8, 0x3> pad_2A089;

    std::array<u8, 0x34> reserved_2A08C;

    bool nfc_enable_flag;
    std::array<u8, 0x3> pad_2A0C1;

    std::array<u8, 0x3C> reserved_2A0C4;

    // nn::settings::system::SleepSettings
    SleepSettings sleep_settings;

    std::array<u8, 0x34> reserved_2A10C;

    // nn::settings::system::EulaVersion
    u32 eula_version_count;
    std::array<u8, 0xC> reserved_2A144;
    std::array<EulaVersion, 32> eula_versions;

    std::array<u8, 0x200> reserved_2A750;

    // nn::settings::system::DeviceNickName
    std::array<u8, 0x80> device_nick_name;

    std::array<u8, 0x80> reserved_2A9D0;

    bool auto_update_enable_flag;
    std::array<u8, 0x3> pad_2AA51;

    std::array<u8, 0x4C> reserved_2AA54;

    // nn::settings::system::BluetoothDevicesSettings
    std::array<std::array<u8, 0x200>, 14> bluetooth_device_settings_last_14;

    std::array<u8, 0x2000> reserved_2C6A0;

    // nn::settings::system::NxControllerSettings
    std::array<std::array<u8, 0x800>, 10> nx_controller_settings_data_from_offset_30;
};

static_assert(offsetof(SystemSettings, language_code) == 0x10);
static_assert(offsetof(SystemSettings, network_setting_count) == 0x50);
static_assert(offsetof(SystemSettings, network_settings_1B0) == 0x60);
static_assert(offsetof(SystemSettings, bluetooth_device_settings_count) == 0x8060);
static_assert(offsetof(SystemSettings, bluetooth_enable_flag) == 0x8064);
static_assert(offsetof(SystemSettings, bluetooth_device_settings_first_10) == 0x8070);
static_assert(offsetof(SystemSettings, ldn_channel) == 0x9470);
static_assert(offsetof(SystemSettings, mii_author_id) == 0x94B0);
static_assert(offsetof(SystemSettings, nx_controller_settings_count) == 0x94F0);
static_assert(offsetof(SystemSettings, nx_controller_legacy_settings) == 0x9500);
static_assert(offsetof(SystemSettings, external_rtc_reset_flag) == 0x98F0);
static_assert(offsetof(SystemSettings, push_notification_activity_mode_on_sleep) == 0x9930);
static_assert(offsetof(SystemSettings, allowed_ssl_host_count) == 0x99F4);
static_assert(offsetof(SystemSettings, host_fs_mount_point) == 0x9A30);
static_assert(offsetof(SystemSettings, allowed_ssl_hosts) == 0x9B30);
static_assert(offsetof(SystemSettings, ble_pairing_settings_count) == 0xA9F0);
static_assert(offsetof(SystemSettings, ble_pairing_settings) == 0xAA00);
static_assert(offsetof(SystemSettings, account_online_storage_settings_count) == 0xAF00);
static_assert(offsetof(SystemSettings, account_online_storage_settings) == 0xAF10);
static_assert(offsetof(SystemSettings, pctl_ready_flag) == 0xB110);
static_assert(offsetof(SystemSettings, theme_id_type0) == 0xB150);
static_assert(offsetof(SystemSettings, chinese_traditional_input_method) == 0xB350);
static_assert(offsetof(SystemSettings, button_config_registered_settings_count) == 0xB3D0);
static_assert(offsetof(SystemSettings, button_config_settings_count) == 0xB3E0);
static_assert(offsetof(SystemSettings, button_config_settings) == 0xB3E8);
static_assert(offsetof(SystemSettings, button_config_registered_settings_embedded) == 0x1D3E0);
static_assert(offsetof(SystemSettings, console_six_axis_sensor_acceleration_bias) == 0x29370);
static_assert(offsetof(SystemSettings, lock_screen_flag) == 0x29470);
static_assert(offsetof(SystemSettings, battery_percentage_flag) == 0x294A0);
static_assert(offsetof(SystemSettings, field_testing_flag) == 0x294C0);
static_assert(offsetof(SystemSettings, backlight_settings_mixed_up) == 0x294F0);
static_assert(offsetof(SystemSettings, user_system_clock_context) == 0x29580);
static_assert(offsetof(SystemSettings, network_system_clock_context) == 0x295A0);
static_assert(offsetof(SystemSettings, user_system_clock_automatic_correction_enabled) == 0x295C0);
static_assert(offsetof(SystemSettings, user_system_clock_automatic_correction_updated_time_point) ==
              0x295C8);
static_assert(offsetof(SystemSettings, account_settings) == 0x295F0);
static_assert(offsetof(SystemSettings, audio_volume_type0) == 0x296F0);
static_assert(offsetof(SystemSettings, hearing_protection_safeguard_remaining_time) == 0x29730);
static_assert(offsetof(SystemSettings, automatic_application_download_flag) == 0x297B0);
static_assert(offsetof(SystemSettings, notification_settings) == 0x297B8);
static_assert(offsetof(SystemSettings, account_notification_settings) == 0x29840);
static_assert(offsetof(SystemSettings, vibration_master_volume) == 0x29A40);
static_assert(offsetof(SystemSettings, analog_stick_user_calibration_left) == 0x29A48);
static_assert(offsetof(SystemSettings, touch_screen_mode) == 0x29A68);
static_assert(offsetof(SystemSettings, edid) == 0x29AA0);
static_assert(offsetof(SystemSettings, data_deletion_settings) == 0x29E80);
static_assert(offsetof(SystemSettings, requires_run_repair_time_reviser) == 0x29ED4);
static_assert(offsetof(SystemSettings, device_time_zone_location_name) == 0x29F40);
static_assert(offsetof(SystemSettings, nfc_enable_flag) == 0x2A0C0);
static_assert(offsetof(SystemSettings, eula_version_count) == 0x2A140);
static_assert(offsetof(SystemSettings, device_nick_name) == 0x2A950);
static_assert(offsetof(SystemSettings, bluetooth_device_settings_last_14) == 0x2AAA0);
static_assert(offsetof(SystemSettings, nx_controller_settings_data_from_offset_30) == 0x2E6A0);

static_assert(sizeof(SystemSettings) == 0x336A0, "SystemSettings has the wrong size!");

SystemSettings DefaultSystemSettings();

} // namespace Service::Set
