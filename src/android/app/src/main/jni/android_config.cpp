// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "android_config.h"
#include "android_settings.h"
#include "common/settings_setting.h"

AndroidConfig::AndroidConfig(const std::string& config_name, ConfigType config_type)
    : Config(config_type) {
    Initialize(config_name);
    if (config_type != ConfigType::InputProfile) {
        ReadAndroidValues();
        SaveAndroidValues();
    }
}

AndroidConfig::~AndroidConfig() {
    if (global) {
        AndroidConfig::SaveAllValues();
    }
}

void AndroidConfig::ReloadAllValues() {
    Reload();
    ReadAndroidValues();
    SaveAndroidValues();
}

void AndroidConfig::SaveAllValues() {
    Save();
    SaveAndroidValues();
}

void AndroidConfig::ReadAndroidValues() {
    if (global) {
        ReadAndroidUIValues();
    }
}

void AndroidConfig::ReadAndroidUIValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::Android));

    ReadCategory(Settings::Category::Android);

    EndGroup();
}

void AndroidConfig::SaveAndroidValues() {
    if (global) {
        SaveAndroidUIValues();
    }

    WriteToIni();
}

void AndroidConfig::SaveAndroidUIValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::Android));

    WriteCategory(Settings::Category::Android);

    EndGroup();
}

std::vector<Settings::BasicSetting*>& AndroidConfig::FindRelevantList(Settings::Category category) {
    auto& map = Settings::values.linkage.by_category;
    if (map.contains(category)) {
        return Settings::values.linkage.by_category[category];
    }
    return AndroidSettings::values.linkage.by_category[category];
}
