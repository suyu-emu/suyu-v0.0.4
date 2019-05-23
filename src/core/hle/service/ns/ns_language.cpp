// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/ns/ns_language.h"

namespace Service::NS {

constexpr ApplicationLanguagePriorityList priority_list_american_english = {{
    ApplicationLanguage::AmericanEnglish,
    ApplicationLanguage::BritishEnglish,
    ApplicationLanguage::LatinAmericanSpanish,
    ApplicationLanguage::CanadianFrench,
    ApplicationLanguage::French,
    ApplicationLanguage::German,
    ApplicationLanguage::Spanish,
    ApplicationLanguage::Italian,
    ApplicationLanguage::Dutch,
    ApplicationLanguage::Portuguese,
    ApplicationLanguage::Russian,
    ApplicationLanguage::Japanese,
    ApplicationLanguage::SimplifiedChinese,
    ApplicationLanguage::TraditionalChinese,
    ApplicationLanguage::Korean,
}};

constexpr ApplicationLanguagePriorityList priority_list_british_english = {{
    ApplicationLanguage::BritishEnglish,
    ApplicationLanguage::AmericanEnglish,
    ApplicationLanguage::French,
    ApplicationLanguage::German,
    ApplicationLanguage::Spanish,
    ApplicationLanguage::Italian,
    ApplicationLanguage::Dutch,
    ApplicationLanguage::Portuguese,
    ApplicationLanguage::Russian,
    ApplicationLanguage::LatinAmericanSpanish,
    ApplicationLanguage::CanadianFrench,
    ApplicationLanguage::Japanese,
    ApplicationLanguage::SimplifiedChinese,
    ApplicationLanguage::TraditionalChinese,
    ApplicationLanguage::Korean,
}};

constexpr ApplicationLanguagePriorityList priority_list_japanese = {{
    ApplicationLanguage::Japanese,
    ApplicationLanguage::AmericanEnglish,
    ApplicationLanguage::BritishEnglish,
    ApplicationLanguage::LatinAmericanSpanish,
    ApplicationLanguage::CanadianFrench,
    ApplicationLanguage::French,
    ApplicationLanguage::German,
    ApplicationLanguage::Spanish,
    ApplicationLanguage::Italian,
    ApplicationLanguage::Dutch,
    ApplicationLanguage::Portuguese,
    ApplicationLanguage::Russian,
    ApplicationLanguage::SimplifiedChinese,
    ApplicationLanguage::TraditionalChinese,
    ApplicationLanguage::Korean,
}};

constexpr ApplicationLanguagePriorityList priority_list_french = {{
    ApplicationLanguage::French,
    ApplicationLanguage::CanadianFrench,
    ApplicationLanguage::BritishEnglish,
    ApplicationLanguage::AmericanEnglish,
    ApplicationLanguage::German,
    ApplicationLanguage::Spanish,
    ApplicationLanguage::Italian,
    ApplicationLanguage::Dutch,
    ApplicationLanguage::Portuguese,
    ApplicationLanguage::Russian,
    ApplicationLanguage::LatinAmericanSpanish,
    ApplicationLanguage::Japanese,
    ApplicationLanguage::SimplifiedChinese,
    ApplicationLanguage::TraditionalChinese,
    ApplicationLanguage::Korean,
}};

constexpr ApplicationLanguagePriorityList priority_list_german = {{
    ApplicationLanguage::German,
    ApplicationLanguage::BritishEnglish,
    ApplicationLanguage::AmericanEnglish,
    ApplicationLanguage::French,
    ApplicationLanguage::Spanish,
    ApplicationLanguage::Italian,
    ApplicationLanguage::Dutch,
    ApplicationLanguage::Portuguese,
    ApplicationLanguage::Russian,
    ApplicationLanguage::LatinAmericanSpanish,
    ApplicationLanguage::CanadianFrench,
    ApplicationLanguage::Japanese,
    ApplicationLanguage::SimplifiedChinese,
    ApplicationLanguage::TraditionalChinese,
    ApplicationLanguage::Korean,
}};

constexpr ApplicationLanguagePriorityList priority_list_latin_american_spanish = {{
    ApplicationLanguage::LatinAmericanSpanish,
    ApplicationLanguage::Spanish,
    ApplicationLanguage::AmericanEnglish,
    ApplicationLanguage::BritishEnglish,
    ApplicationLanguage::Portuguese,
    ApplicationLanguage::CanadianFrench,
    ApplicationLanguage::French,
    ApplicationLanguage::Italian,
    ApplicationLanguage::German,
    ApplicationLanguage::Dutch,
    ApplicationLanguage::Russian,
    ApplicationLanguage::Japanese,
    ApplicationLanguage::SimplifiedChinese,
    ApplicationLanguage::TraditionalChinese,
    ApplicationLanguage::Korean,
}};

constexpr ApplicationLanguagePriorityList priority_list_spanish = {{
    ApplicationLanguage::Spanish,
    ApplicationLanguage::LatinAmericanSpanish,
    ApplicationLanguage::BritishEnglish,
    ApplicationLanguage::AmericanEnglish,
    ApplicationLanguage::French,
    ApplicationLanguage::German,
    ApplicationLanguage::Italian,
    ApplicationLanguage::Dutch,
    ApplicationLanguage::Portuguese,
    ApplicationLanguage::Russian,
    ApplicationLanguage::CanadianFrench,
    ApplicationLanguage::Japanese,
    ApplicationLanguage::SimplifiedChinese,
    ApplicationLanguage::TraditionalChinese,
    ApplicationLanguage::Korean,
}};

constexpr ApplicationLanguagePriorityList priority_list_italian = {{
    ApplicationLanguage::Italian,
    ApplicationLanguage::BritishEnglish,
    ApplicationLanguage::AmericanEnglish,
    ApplicationLanguage::French,
    ApplicationLanguage::German,
    ApplicationLanguage::Spanish,
    ApplicationLanguage::Dutch,
    ApplicationLanguage::Portuguese,
    ApplicationLanguage::Russian,
    ApplicationLanguage::LatinAmericanSpanish,
    ApplicationLanguage::CanadianFrench,
    ApplicationLanguage::Japanese,
    ApplicationLanguage::SimplifiedChinese,
    ApplicationLanguage::TraditionalChinese,
    ApplicationLanguage::Korean,
}};

constexpr ApplicationLanguagePriorityList priority_list_dutch = {{
    ApplicationLanguage::Dutch,
    ApplicationLanguage::BritishEnglish,
    ApplicationLanguage::AmericanEnglish,
    ApplicationLanguage::French,
    ApplicationLanguage::German,
    ApplicationLanguage::Spanish,
    ApplicationLanguage::Italian,
    ApplicationLanguage::Portuguese,
    ApplicationLanguage::Russian,
    ApplicationLanguage::LatinAmericanSpanish,
    ApplicationLanguage::CanadianFrench,
    ApplicationLanguage::Japanese,
    ApplicationLanguage::SimplifiedChinese,
    ApplicationLanguage::TraditionalChinese,
    ApplicationLanguage::Korean,
}};

constexpr ApplicationLanguagePriorityList priority_list_canadian_french = {{
    ApplicationLanguage::CanadianFrench,
    ApplicationLanguage::French,
    ApplicationLanguage::AmericanEnglish,
    ApplicationLanguage::BritishEnglish,
    ApplicationLanguage::LatinAmericanSpanish,
    ApplicationLanguage::Spanish,
    ApplicationLanguage::German,
    ApplicationLanguage::Italian,
    ApplicationLanguage::Dutch,
    ApplicationLanguage::Portuguese,
    ApplicationLanguage::Russian,
    ApplicationLanguage::Japanese,
    ApplicationLanguage::SimplifiedChinese,
    ApplicationLanguage::TraditionalChinese,
    ApplicationLanguage::Korean,
}};

constexpr ApplicationLanguagePriorityList priority_list_portuguese = {{
    ApplicationLanguage::Portuguese,
    ApplicationLanguage::BritishEnglish,
    ApplicationLanguage::AmericanEnglish,
    ApplicationLanguage::French,
    ApplicationLanguage::German,
    ApplicationLanguage::Spanish,
    ApplicationLanguage::Italian,
    ApplicationLanguage::Dutch,
    ApplicationLanguage::Russian,
    ApplicationLanguage::LatinAmericanSpanish,
    ApplicationLanguage::CanadianFrench,
    ApplicationLanguage::Japanese,
    ApplicationLanguage::SimplifiedChinese,
    ApplicationLanguage::TraditionalChinese,
    ApplicationLanguage::Korean,
}};

constexpr ApplicationLanguagePriorityList priority_list_russian = {{
    ApplicationLanguage::Russian,
    ApplicationLanguage::BritishEnglish,
    ApplicationLanguage::AmericanEnglish,
    ApplicationLanguage::French,
    ApplicationLanguage::German,
    ApplicationLanguage::Spanish,
    ApplicationLanguage::Italian,
    ApplicationLanguage::Dutch,
    ApplicationLanguage::Portuguese,
    ApplicationLanguage::LatinAmericanSpanish,
    ApplicationLanguage::CanadianFrench,
    ApplicationLanguage::Japanese,
    ApplicationLanguage::SimplifiedChinese,
    ApplicationLanguage::TraditionalChinese,
    ApplicationLanguage::Korean,
}};

constexpr ApplicationLanguagePriorityList priority_list_korean = {{
    ApplicationLanguage::Korean,
    ApplicationLanguage::AmericanEnglish,
    ApplicationLanguage::BritishEnglish,
    ApplicationLanguage::LatinAmericanSpanish,
    ApplicationLanguage::CanadianFrench,
    ApplicationLanguage::French,
    ApplicationLanguage::German,
    ApplicationLanguage::Spanish,
    ApplicationLanguage::Italian,
    ApplicationLanguage::Dutch,
    ApplicationLanguage::Portuguese,
    ApplicationLanguage::Russian,
    ApplicationLanguage::Japanese,
    ApplicationLanguage::SimplifiedChinese,
    ApplicationLanguage::TraditionalChinese,
}};

constexpr ApplicationLanguagePriorityList priority_list_traditional_chinese = {{
    ApplicationLanguage::TraditionalChinese,
    ApplicationLanguage::SimplifiedChinese,
    ApplicationLanguage::AmericanEnglish,
    ApplicationLanguage::BritishEnglish,
    ApplicationLanguage::Japanese,
    ApplicationLanguage::LatinAmericanSpanish,
    ApplicationLanguage::CanadianFrench,
    ApplicationLanguage::French,
    ApplicationLanguage::German,
    ApplicationLanguage::Spanish,
    ApplicationLanguage::Italian,
    ApplicationLanguage::Dutch,
    ApplicationLanguage::Portuguese,
    ApplicationLanguage::Russian,
    ApplicationLanguage::Korean,
}};

constexpr ApplicationLanguagePriorityList priority_list_simplified_chinese = {{
    ApplicationLanguage::SimplifiedChinese,
    ApplicationLanguage::TraditionalChinese,
    ApplicationLanguage::AmericanEnglish,
    ApplicationLanguage::BritishEnglish,
    ApplicationLanguage::Japanese,
    ApplicationLanguage::LatinAmericanSpanish,
    ApplicationLanguage::CanadianFrench,
    ApplicationLanguage::French,
    ApplicationLanguage::German,
    ApplicationLanguage::Spanish,
    ApplicationLanguage::Italian,
    ApplicationLanguage::Dutch,
    ApplicationLanguage::Portuguese,
    ApplicationLanguage::Russian,
    ApplicationLanguage::Korean,
}};

const ApplicationLanguagePriorityList* GetApplicationLanguagePriorityList(ApplicationLanguage lang) {
    switch (lang) {
    case ApplicationLanguage::AmericanEnglish:
        return &priority_list_american_english;
    case ApplicationLanguage::BritishEnglish:
        return &priority_list_british_english;
    case ApplicationLanguage::Japanese:
        return &priority_list_japanese;
    case ApplicationLanguage::French:
        return &priority_list_french;
    case ApplicationLanguage::German:
        return &priority_list_german;
    case ApplicationLanguage::LatinAmericanSpanish:
        return &priority_list_latin_american_spanish;
    case ApplicationLanguage::Spanish:
        return &priority_list_spanish;
    case ApplicationLanguage::Italian:
        return &priority_list_italian;
    case ApplicationLanguage::Dutch:
        return &priority_list_dutch;
    case ApplicationLanguage::CanadianFrench:
        return &priority_list_canadian_french;
    case ApplicationLanguage::Portuguese:
        return &priority_list_portuguese;
    case ApplicationLanguage::Russian:
        return &priority_list_russian;
    case ApplicationLanguage::Korean:
        return &priority_list_korean;
    case ApplicationLanguage::TraditionalChinese:
        return &priority_list_traditional_chinese;
    case ApplicationLanguage::SimplifiedChinese:
        return &priority_list_simplified_chinese;
    default:
        return nullptr;
    }
}

std::optional<ApplicationLanguage> ConvertToApplicationLanguage(
    const Service::Set::LanguageCode language_code) {
    switch (language_code) {
    case Service::Set::LanguageCode::EN_US:
        return ApplicationLanguage::AmericanEnglish;
    case Service::Set::LanguageCode::EN_GB:
        return ApplicationLanguage::BritishEnglish;
    case Service::Set::LanguageCode::JA:
        return ApplicationLanguage::Japanese;
    case Service::Set::LanguageCode::FR:
        return ApplicationLanguage::French;
    case Service::Set::LanguageCode::DE:
        return ApplicationLanguage::German;
    case Service::Set::LanguageCode::ES_419:
        return ApplicationLanguage::LatinAmericanSpanish;
    case Service::Set::LanguageCode::ES:
        return ApplicationLanguage::Spanish;
    case Service::Set::LanguageCode::IT:
        return ApplicationLanguage::Italian;
    case Service::Set::LanguageCode::NL:
        return ApplicationLanguage::Dutch;
    case Service::Set::LanguageCode::FR_CA:
        return ApplicationLanguage::CanadianFrench;
    case Service::Set::LanguageCode::PT:
        return ApplicationLanguage::Portuguese;
    case Service::Set::LanguageCode::RU:
        return ApplicationLanguage::Russian;
    case Service::Set::LanguageCode::KO:
        return ApplicationLanguage::Korean;
    case Service::Set::LanguageCode::ZH_HANT:
        return ApplicationLanguage::TraditionalChinese;
    case Service::Set::LanguageCode::ZH_HANS:
        return ApplicationLanguage::SimplifiedChinese;
    default:
        return std::nullopt;
    }
}

std::optional<Service::Set::LanguageCode> ConvertToLanguageCode(const ApplicationLanguage lang) {
    switch (lang) {
    case ApplicationLanguage::AmericanEnglish:
        return Service::Set::LanguageCode::EN_US;
    case ApplicationLanguage::BritishEnglish:
        return Service::Set::LanguageCode::EN_GB;
    case ApplicationLanguage::Japanese:
        return Service::Set::LanguageCode::JA;
    case ApplicationLanguage::French:
        return Service::Set::LanguageCode::FR;
    case ApplicationLanguage::German:
        return Service::Set::LanguageCode::DE;
    case ApplicationLanguage::LatinAmericanSpanish:
        return Service::Set::LanguageCode::ES_419;
    case ApplicationLanguage::Spanish:
        return Service::Set::LanguageCode::ES;
    case ApplicationLanguage::Italian:
        return Service::Set::LanguageCode::IT;
    case ApplicationLanguage::Dutch:
        return Service::Set::LanguageCode::NL;
    case ApplicationLanguage::CanadianFrench:
        return Service::Set::LanguageCode::FR_CA;
    case ApplicationLanguage::Portuguese:
        return Service::Set::LanguageCode::PT;
    case ApplicationLanguage::Russian:
        return Service::Set::LanguageCode::RU;
    case ApplicationLanguage::Korean:
        return Service::Set::LanguageCode::KO;
    case ApplicationLanguage::TraditionalChinese:
        return Service::Set::LanguageCode::ZH_HANT;
    case ApplicationLanguage::SimplifiedChinese:
        return Service::Set::LanguageCode::ZH_HANS;
    default:
        return std::nullopt;
    }
}
} // namespace Service::NS