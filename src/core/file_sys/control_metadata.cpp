// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <span>
#include <zlib.h>

#include "common/settings.h"
#include "common/settings_enums.h"
#include "common/string_util.h"
#include "common/swap.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/vfs/vfs.h"

namespace FileSys {

const std::array<const char*, size_t(Language::Count)> LANGUAGE_NAMES{{
    "AmericanEnglish",
    "BritishEnglish",
    "Japanese",
    "French",
    "German",
    "LatinAmericanSpanish",
    "Spanish",
    "Italian",
    "Dutch",
    "CanadianFrench",
    "Portuguese",
    "Russian",
    "Korean",
    "TraditionalChinese",
    "SimplifiedChinese",
    "BrazilianPortuguese",
    "Polish",
    "Thai",
}};

namespace
{
    constexpr std::size_t MAX_EXPANDED_LANG_SIZE = sizeof(LanguageEntry) * 32;


    bool InflateRawDeflate(std::span<const u8> compressed, std::vector<u8>& out)
    {
        if (compressed.empty()) return false;

        z_stream stream{};
        stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(compressed.data()));
        stream.avail_in = static_cast<uInt>(compressed.size());
        if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
            return false;
        }

        out.resize(MAX_EXPANDED_LANG_SIZE);
        stream.next_out = reinterpret_cast<Bytef*>(out.data());
        stream.avail_out = static_cast<uInt>(out.size());

        int ret = inflate(&stream, Z_FINISH);
        inflateEnd(&stream);

        if (ret != Z_STREAM_END && ret != Z_OK) {
            return false;
        }

        // Shrink to actual decompressed size
        out.resize(stream.total_out);
        return true;
    }
} // namespace

std::string LanguageEntry::GetApplicationName() const {
    return Common::StringFromFixedZeroTerminatedBuffer(application_name.data(), application_name.size());
}

std::string LanguageEntry::GetDeveloperName() const {
    return Common::StringFromFixedZeroTerminatedBuffer(developer_name.data(), developer_name.size());
}

NACP::NACP() = default;

NACP::NACP(VirtualFile file)
{
    file->ReadObject(&raw);
    if (raw.titles_data_format == TitleDataFormat::Compressed) {
        const u16 compressed_size = raw.language_entries.compressed_data.buffer_size;
        std::span<const u8> compressed_payload{raw.language_entries.compressed_data.buffer,
                                               compressed_size};

        std::vector<u8> decompressed;
        if (InflateRawDeflate(compressed_payload, decompressed)) {
            const size_t entry_count = decompressed.size() / sizeof(LanguageEntry);
            language_entries.resize(entry_count);
            std::memcpy(language_entries.data(), decompressed.data(), decompressed.size());
        }
    } else {
        language_entries.resize(16);
        std::memcpy(language_entries.data(), raw.language_entries.language_entries.data(),
                    sizeof(raw.language_entries.language_entries));
    }
}

NACP::~NACP() = default;

const LanguageEntry& NACP::GetLanguageEntry() const {
    u32 index = static_cast<u32>(Settings::values.language_index.GetValue());

    if (index < language_entries.size()) {
        return language_entries[index];
    }

    for (const auto& entry : language_entries) {
        return entry;
    }

    return language_entries.at(static_cast<u8>(Language::AmericanEnglish));
}

std::vector<std::string> NACP::GetApplicationNames() const {
    std::vector<std::string> names;
    names.reserve(language_entries.size());
    for (const auto& entry : language_entries) {
        names.push_back(entry.GetApplicationName());
    }
    return names;
}

std::string NACP::GetApplicationName() const {
    return GetLanguageEntry().GetApplicationName();
}

std::string NACP::GetDeveloperName() const {
    return GetLanguageEntry().GetDeveloperName();
}

u64 NACP::GetTitleId() const {
    return raw.save_data_owner_id;
}

u64 NACP::GetDLCBaseTitleId() const {
    return raw.dlc_base_title_id;
}

std::string NACP::GetVersionString() const {
    return Common::StringFromFixedZeroTerminatedBuffer(raw.version_string.data(),
                                                       raw.version_string.size());
}

u64 NACP::GetDefaultNormalSaveSize() const {
    return raw.user_account_save_data_size;
}

u64 NACP::GetDefaultJournalSaveSize() const {
    return raw.user_account_save_data_journal_size;
}

bool NACP::GetUserAccountSwitchLock() const {
    return raw.user_account_switch_lock != 0;
}

u32 NACP::GetSupportedLanguages() const {
    return u32(raw.supported_languages);
}

u64 NACP::GetDeviceSaveDataSize() const {
    return raw.device_save_data_size;
}

u32 NACP::GetParentalControlFlag() const {
    return raw.parental_control;
}

const std::array<u8, 0x20>& NACP::GetRatingAge() const {
    return raw.rating_age;
}

std::vector<u8> NACP::GetRawBytes() const {
    std::vector<u8> out(sizeof(RawNACP));
    std::memcpy(out.data(), &raw, sizeof(RawNACP));
    return out;
}
} // namespace FileSys
