// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"
#include "core/hle/service/set/system_settings.h"

namespace Core {
class System;
}

namespace Service::Set {
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
