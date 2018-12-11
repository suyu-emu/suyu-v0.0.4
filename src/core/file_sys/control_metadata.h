// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <string>
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/swap.h"
#include "core/file_sys/vfs.h"

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
    INSERT_PADDING_BYTES(2);
    u32_le application_attribute;
    u32_le supported_languages;
    u32_le parental_control;
    bool screenshot_enabled;
    u8 video_capture_mode;
    bool data_loss_confirmation;
    INSERT_PADDING_BYTES(1);
    u64_le title_id;
    std::array<u8, 0x20> rating_age;
    std::array<char, 0x10> version_string;
    u64_le dlc_base_title_id;
    u64_le title_id_2;
    u64_le normal_save_data_size;
    u64_le journal_sava_data_size;
    INSERT_PADDING_BYTES(0x18);
    u64_le product_code;
    std::array<u64_le, 0x8> local_communication;
    u8 logo_type;
    u8 logo_handling;
    bool runtime_add_on_content_install;
    INSERT_PADDING_BYTES(5);
    u64_le title_id_update;
    std::array<u8, 0x40> bcat_passphrase;
    INSERT_PADDING_BYTES(0xEC0);
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
    Portugese = 10,
    Russian = 11,
    Korean = 12,
    Taiwanese = 13,
    Chinese = 14,

    Default = 255,
};

extern const std::array<const char*, 15> LANGUAGE_NAMES;

// A class representing the format used by NX metadata files, typically named Control.nacp.
// These store application name, dev name, title id, and other miscellaneous data.
class NACP {
public:
    explicit NACP();
    explicit NACP(VirtualFile file);
    ~NACP();

    const LanguageEntry& GetLanguageEntry(Language language = Language::Default) const;
    std::string GetApplicationName(Language language = Language::Default) const;
    std::string GetDeveloperName(Language language = Language::Default) const;
    u64 GetTitleId() const;
    u64 GetDLCBaseTitleId() const;
    std::string GetVersionString() const;
    u64 GetDefaultNormalSaveSize() const;
    u64 GetDefaultJournalSaveSize() const;
    std::vector<u8> GetRawBytes() const;

private:
    RawNACP raw{};
};

} // namespace FileSys
