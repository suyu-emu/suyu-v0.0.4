// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <string>
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
static_assert(sizeof(LanguageEntry) == 0x300, "LanguageEntry has incorrect size.");

// The raw file format of a NACP file.
struct RawNACP {
    std::array<LanguageEntry, 16> language_entries;
    std::array<u8, 0x25> isbn;
    u8 startup_user_account;
    u8 user_account_switch_lock;
    u8 addon_content_registration_type;
    u32_le application_attribute;
    u32_le supported_languages;
    u32_le parental_control;
    bool screenshot_enabled;
    u8 video_capture_mode;
    bool data_loss_confirmation;
    INSERT_PADDING_BYTES(1);
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
    INSERT_PADDING_BYTES(5);
    u64_le seed_for_pseudo_device_id;
    std::array<u8, 0x41> bcat_passphrase;
    INSERT_PADDING_BYTES(7);
    u64_le user_account_save_data_max_size;
    u64_le user_account_save_data_max_journal_size;
    u64_le device_save_data_max_size;
    u64_le device_save_data_max_journal_size;
    u64_le temporary_storage_size;
    u64_le cache_storage_size;
    u64_le cache_storage_journal_size;
    u64_le cache_storage_data_and_journal_max_size;
    u16_le cache_storage_max_index;
    INSERT_PADDING_BYTES(0x8B);
    u8 app_error_code_prefix;
    INSERT_PADDING_BYTES(1);
    u8 acd_index;
    u8 apparent_platform;
    INSERT_PADDING_BYTES(0x22F);
    std::array<u8, 0x89> app_control_data_condition;
    u8 initial_program_index;
    INSERT_PADDING_BYTES(2);
    u32_le accessible_program_index_flags;
    u8 album_file_export;
    INSERT_PADDING_BYTES(7);
    std::array<u8, 0x80> save_data_certificate_bytes;
    u8 has_ingame_voice_chat;
    INSERT_PADDING_BYTES(3);
    u32_le supported_extra_addon_content_flag;
    INSERT_PADDING_BYTES(0x698);
    std::array<u8, 0x400> platform_specific_region;
};
static_assert(sizeof(RawNACP) == 0x4000, "RawNACP has incorrect size.");

// A language on the NX. These are for names and icons.
enum class Language : u8 {
    AmericanEnglish = 0,
    BritishEnglish = 1,
    Japanese = 2,
    French = 3,
    German = 4,
    LatinAmericanSpanish = 5,
    Spanish = 6,
    Italian = 7,
    Dutch = 8,
    CanadianFrench = 9,
    Portuguese = 10,
    Russian = 11,
    Korean = 12,
    TraditionalChinese = 13,
    SimplifiedChinese = 14,
    BrazilianPortuguese = 15,

    Default = 255,
};

extern const std::array<const char*, 16> LANGUAGE_NAMES;

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
    std::vector<u8> GetRawBytes() const;
    bool GetUserAccountSwitchLock() const;
    u64 GetDeviceSaveDataSize() const;
    u32 GetParentalControlFlag() const;
    const std::array<u8, 0x20>& GetRatingAge() const;

private:
    RawNACP raw{};
};

} // namespace FileSys
