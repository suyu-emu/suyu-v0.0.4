// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

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

enum class KeyboardLayout : u64 {
    Japanese = 0,
    EnglishUs = 1,
    EnglishUsInternational = 2,
    EnglishUk = 3,
    French = 4,
    FrenchCa = 5,
    Spanish = 6,
    SpanishLatin = 7,
    German = 8,
    Italian = 9,
    Portuguese = 10,
    Russian = 11,
    Korean = 12,
    ChineseSimplified = 13,
    ChineseTraditional = 14,
};

constexpr std::array<LanguageCode, 18> available_language_codes = {{
    LanguageCode::JA,
    LanguageCode::EN_US,
    LanguageCode::FR,
    LanguageCode::DE,
    LanguageCode::IT,
    LanguageCode::ES,
    LanguageCode::ZH_CN,
    LanguageCode::KO,
    LanguageCode::NL,
    LanguageCode::PT,
    LanguageCode::RU,
    LanguageCode::ZH_TW,
    LanguageCode::EN_GB,
    LanguageCode::FR_CA,
    LanguageCode::ES_419,
    LanguageCode::ZH_HANS,
    LanguageCode::ZH_HANT,
    LanguageCode::PT_BR,
}};

static constexpr std::array<std::pair<LanguageCode, KeyboardLayout>, 18> language_to_layout{{
    {LanguageCode::JA, KeyboardLayout::Japanese},
    {LanguageCode::EN_US, KeyboardLayout::EnglishUs},
    {LanguageCode::FR, KeyboardLayout::French},
    {LanguageCode::DE, KeyboardLayout::German},
    {LanguageCode::IT, KeyboardLayout::Italian},
    {LanguageCode::ES, KeyboardLayout::Spanish},
    {LanguageCode::ZH_CN, KeyboardLayout::ChineseSimplified},
    {LanguageCode::KO, KeyboardLayout::Korean},
    {LanguageCode::NL, KeyboardLayout::EnglishUsInternational},
    {LanguageCode::PT, KeyboardLayout::Portuguese},
    {LanguageCode::RU, KeyboardLayout::Russian},
    {LanguageCode::ZH_TW, KeyboardLayout::ChineseTraditional},
    {LanguageCode::EN_GB, KeyboardLayout::EnglishUk},
    {LanguageCode::FR_CA, KeyboardLayout::FrenchCa},
    {LanguageCode::ES_419, KeyboardLayout::SpanishLatin},
    {LanguageCode::ZH_HANS, KeyboardLayout::ChineseSimplified},
    {LanguageCode::ZH_HANT, KeyboardLayout::ChineseTraditional},
    {LanguageCode::PT_BR, KeyboardLayout::Portuguese},
}};

LanguageCode GetLanguageCodeFromIndex(std::size_t idx);

class SET final : public ServiceFramework<SET> {
public:
    explicit SET(Core::System& system_);
    ~SET() override;

private:
    void GetLanguageCode(HLERequestContext& ctx);
    void GetAvailableLanguageCodes(HLERequestContext& ctx);
    void MakeLanguageCode(HLERequestContext& ctx);
    void GetAvailableLanguageCodes2(HLERequestContext& ctx);
    void GetAvailableLanguageCodeCount(HLERequestContext& ctx);
    void GetAvailableLanguageCodeCount2(HLERequestContext& ctx);
    void GetQuestFlag(HLERequestContext& ctx);
    void GetRegionCode(HLERequestContext& ctx);
    void GetKeyCodeMap(HLERequestContext& ctx);
    void GetKeyCodeMap2(HLERequestContext& ctx);
    void GetDeviceNickName(HLERequestContext& ctx);
};

} // namespace Service::Set
