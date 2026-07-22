// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <string>
#include <vector>
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/swap.h"
#include "core/file_sys/vfs/vfs_types.h"

namespace FileSys {

// A localized entry containing strings within the NACP.
// One for each language of type Language.
struct LanguageEntry {
    std::array<char, 0x200> application_name;
    std::array<char, 0x100> developer_name;

    std::string GetApplicationName() const;
    std::string GetDeveloperName() const;
};
static_assert(sizeof(LanguageEntry) == 0x300);

struct LanguageEntryData {
    union {
        // TitleDataFormat::Uncompressed (16 entries)
        std::array<LanguageEntry, 16> language_entries;

        // TitleDataFormat::Compressed (18+ entries)
        struct {
            u16 buffer_size;
            u8 buffer[0x2FFE];
        } compressed_data;
    };
};

enum class TitleDataFormat : u8 {
    Uncompressed = 0,
    Compressed = 1,
};

struct ApplicationNeighborDetectionGroupConfiguration {
    u64 group_id; ///< GroupId
    std::array<u8, 0x10> key;
};
static_assert(sizeof(ApplicationNeighborDetectionGroupConfiguration) == 0x18);

// NeighborDetectionClientConfiguration
struct NeighborDetectionClientConfiguration {
    ApplicationNeighborDetectionGroupConfiguration send_group_configuration; ///< SendGroupConfiguration
    std::array<ApplicationNeighborDetectionGroupConfiguration, 0x10> receivable_group_configurations; ///< ReceivableGroupConfigurations
};
static_assert(sizeof(NeighborDetectionClientConfiguration) == 0x198);

enum class ApparentPlatform : u8 {
    NX = 0,
    Ounce = 1,
    Count = 2
};

enum class JitConfigurationFlag : u64 {
    None = 0,
    IsEnabled = 1,
};

struct JitConfiguration {
    JitConfigurationFlag jit_configuration_flag;
    u64 memory_size;
};
static_assert(sizeof(JitConfiguration) == 0x10);

enum RequiredAddOnContentsSetDescriptorFlag : u16 {
    None = 0,
    Continue = 1
};
struct RequiredAddOnContentsSetDescriptor {
    u16 index : 15;
    RequiredAddOnContentsSetDescriptorFlag flag : 1;
};
static_assert(sizeof(RequiredAddOnContentsSetDescriptor) == 0x2);

struct RequiredAddOnContentsSetBinaryDescriptor {
    RequiredAddOnContentsSetDescriptor descriptors[0x20];
};
static_assert(sizeof(RequiredAddOnContentsSetBinaryDescriptor) == 0x40);

enum class PlayReportPermission : u8 {
    None = 0,
    TargetMarketing = 1,
};

enum class CrashScreenshotForProd : u8 {
    Deny  = 0,
    Allow = 1,
};

enum class CrashScreenshotForDev : u8 {
    Deny  = 0,
    Allow = 1,
};

enum class ContentsAvailabilityTransitionPolicy : u8 {
    NoPolicy = 0,
    Stable = 1,
    Changeable = 2,
};

struct AccessibleLaunchRequiredVersion {
    std::array<u64, 8> application_id;
};
static_assert(sizeof(AccessibleLaunchRequiredVersion) == 0x40);

enum class CrashReport : u8 {
    Deny  = 0,
    Allow = 1,
};

enum class PlayLogQueryCapability : u8 {
    None = 0,
    WhiteList = 1,
    All = 2,
};

struct ApplicationControlDataConditionData {
    u8 priority;
    INSERT_PADDING_BYTES(7);
    u16 aoc_index;
    INSERT_PADDING_BYTES(6);
};
static_assert(sizeof(ApplicationControlDataConditionData) == 0x10);

#pragma pack(push, 1)
struct ApplicationControlDataCondition {
    u64 type;
    std::array<ApplicationControlDataConditionData, 0x8> data;
    u8 count;
};
#pragma pack(pop)
static_assert(sizeof(ApplicationControlDataCondition) == 0x89);

// A language on the NX. These are for names and icons.
#define NACP_LANGUAGE_LIST \
    NACP_LANGUAGE_ELEM(AmericanEnglish, 0) \
    NACP_LANGUAGE_ELEM(BritishEnglish, 1) \
    NACP_LANGUAGE_ELEM(Japanese, 2) \
    NACP_LANGUAGE_ELEM(French, 3) \
    NACP_LANGUAGE_ELEM(German, 4) \
    NACP_LANGUAGE_ELEM(LatinAmericanSpanish, 5) \
    NACP_LANGUAGE_ELEM(Spanish, 6) \
    NACP_LANGUAGE_ELEM(Italian, 7) \
    NACP_LANGUAGE_ELEM(Dutch, 8) \
    NACP_LANGUAGE_ELEM(CanadianFrench, 9) \
    NACP_LANGUAGE_ELEM(Portuguese, 10) \
    NACP_LANGUAGE_ELEM(Russian, 11) \
    NACP_LANGUAGE_ELEM(Korean, 12) \
    NACP_LANGUAGE_ELEM(TraditionalChinese, 13) \
    NACP_LANGUAGE_ELEM(SimplifiedChinese, 14) \
    NACP_LANGUAGE_ELEM(BrazilianPortuguese, 15) \
    NACP_LANGUAGE_ELEM(Polish, 16) \
    NACP_LANGUAGE_ELEM(Thai, 17) \

enum class Language : u8 {
#define NACP_LANGUAGE_ELEM(X, N) X = N,
    NACP_LANGUAGE_LIST
#undef NACP_LANGUAGE_ELEM
    Count = 18,
    Default = 255,
};

// Yes it's duplicated, why? Bits i guess
enum class SupportedLanguage : u32 {
#define NACP_LANGUAGE_ELEM(X, N) X = 1ULL << N,
    NACP_LANGUAGE_LIST
#undef NACP_LANGUAGE_ELEM
};
#undef NACP_LANGUAGE_LIST

// The raw file format of a NACP file.
struct RawNACP {
    LanguageEntryData language_entries;
    std::array<u8, 0x25> isbn;
    u8 startup_user_account;
    u8 user_account_switch_lock;
    u8 addon_content_registration_type;
    u32_le application_attribute;
    SupportedLanguage supported_languages;
    u32_le parental_control;
    bool screenshot_enabled;
    u8 video_capture_mode;
    bool data_loss_confirmation;
    u8 play_log_policy;
    u64_le presence_group_id;
    std::array<u8, 0x20> rating_age;
    std::array<char, 0x10> version_string;
    u64_le dlc_base_title_id;
    u64_le save_data_owner_id;
    u64_le user_account_save_data_size;
    u64_le user_account_save_data_journal_size;
    u64_le device_save_data_size;
    u64_le device_save_data_journal_size;
    u64_le bcat_delivery_cache_storage_size;
    char application_error_code_category[8];
    std::array<u64_le, 0x8> local_communication;
    u8 logo_type;
    u8 logo_handling;
    bool runtime_add_on_content_install;
    u8 runtime_parameter_delivery;
    u8 appropriate_age_for_china;
    INSERT_PADDING_BYTES(1);
    CrashReport crash_report;
    u64_le seed_for_pseudo_device_id;
    std::array<u8, 0x41> bcat_passphrase;
    u8 startup_user_account_option;
    INSERT_PADDING_BYTES(6); // ReservedForUserAccountSaveDataOperation
    u64_le user_account_save_data_max_size;
    u64_le user_account_save_data_max_journal_size;
    u64_le device_save_data_max_size;
    u64_le device_save_data_max_journal_size;
    u64_le temporary_storage_size;
    u64_le cache_storage_size;
    u64_le cache_storage_journal_size;
    u64_le cache_storage_data_and_journal_max_size;
    u16_le cache_storage_max_index;
    u8 runtime_upgrade;
    u32_le supporting_limited_application_licenses;
    std::array<u8, 0x8*16> play_log_queryable_application_id;
    PlayLogQueryCapability play_log_query_capability;
    u8 repair_flag;
    u8 program_index;
    u8 required_network_service_license_on_launch_flag;
    u8 app_error_code_prefix;
    TitleDataFormat titles_data_format;
    u8 acd_index;
    ApparentPlatform apparent_platform;
    // NeighborDetectionClientConfiguration neighbor_detection_client_configuration;
    // JitConfiguration jit_configuration;
    // RequiredAddOnContentsSetBinaryDescriptor required_add_on_contents_set_binary_descriptor;
    // PlayReportPermission play_report_permission;
    // CrashScreenshotForProd crash_screenshot_for_prod;
    // CrashScreenshotForDev crash_screenshot_for_dev;
    // ContentsAvailabilityTransitionPolicy contents_availability_transition_policy;
    // SupportedLanguage supported_language_copy; ///< TODO: add to XML generation.
    INSERT_PADDING_BYTES(0x1EF);
    AccessibleLaunchRequiredVersion accessible_launch_required_version;
    ApplicationControlDataCondition application_control_data_condition; ///< Used for Switch 2 upgrade packs, which are distributed as AddOnContent titles
    u8 initial_program_index;
    INSERT_PADDING_BYTES(2);
    u32_le accessible_program_index_flags;
    u8 album_file_export;
    INSERT_PADDING_BYTES(7);
    std::array<u8, 0x80> save_data_certificate_bytes;
    u8 has_ingame_voice_chat;
    INSERT_PADDING_BYTES(3);
    u32_le supported_extra_addon_content_flag;
    u8 has_karaoke_feature;
    INSERT_PADDING_BYTES(0x697);
    std::array<u8, 0x400> platform_specific_region;
};
static_assert(sizeof(RawNACP) == 0x4000, "RawNACP has incorrect size.");

extern const std::array<const char*, size_t(Language::Count)> LANGUAGE_NAMES;

// A class representing the format used by NX metadata files, typically named Control.nacp.
// These store application name, dev name, title id, and other miscellaneous data.
class NACP {
public:
    explicit NACP();
    explicit NACP(VirtualFile file);
    ~NACP();

    const LanguageEntry& GetLanguageEntry() const;
    std::string GetApplicationName() const;
    std::string GetDeveloperName() const;
    u64 GetTitleId() const;
    u64 GetDLCBaseTitleId() const;
    std::string GetVersionString() const;
    u64 GetDefaultNormalSaveSize() const;
    u64 GetDefaultJournalSaveSize() const;
    u32 GetSupportedLanguages() const;
    std::vector<std::string> GetApplicationNames() const;
    std::vector<u8> GetRawBytes() const;
    bool GetUserAccountSwitchLock() const;
    u64 GetDeviceSaveDataSize() const;
    u32 GetParentalControlFlag() const;
    const std::array<u8, 0x20>& GetRatingAge() const;

private:
    RawNACP raw{};
    std::vector<LanguageEntry> language_entries;
};

} // namespace FileSys
