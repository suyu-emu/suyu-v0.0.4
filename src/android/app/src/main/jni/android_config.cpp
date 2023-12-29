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
        ReadUIValues();
    }
    ReadDriverValues();
}

void AndroidConfig::ReadAndroidUIValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::Android));

    ReadCategory(Settings::Category::Android);

    EndGroup();
}

void AndroidConfig::ReadUIValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::Ui));

    ReadPathValues();

    EndGroup();
}

void AndroidConfig::ReadPathValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::Paths));

    AndroidSettings::values.game_dirs.clear();
    const int gamedirs_size = BeginArray(std::string("gamedirs"));
    for (int i = 0; i < gamedirs_size; ++i) {
        SetArrayIndex(i);
        AndroidSettings::GameDir game_dir;
        game_dir.path = ReadStringSetting(std::string("path"));
        game_dir.deep_scan =
            ReadBooleanSetting(std::string("deep_scan"), std::make_optional(false));
        AndroidSettings::values.game_dirs.push_back(game_dir);
    }
    EndArray();

    EndGroup();
}

void AndroidConfig::ReadDriverValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::GpuDriver));

    ReadCategory(Settings::Category::GpuDriver);

    EndGroup();
}

void AndroidConfig::SaveAndroidValues() {
    if (global) {
        SaveAndroidUIValues();
        SaveUIValues();
    }
    SaveDriverValues();

    WriteToIni();
}

void AndroidConfig::SaveAndroidUIValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::Android));

    WriteCategory(Settings::Category::Android);

    EndGroup();
}

void AndroidConfig::SaveUIValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::Ui));

    SavePathValues();

    EndGroup();
}

void AndroidConfig::SavePathValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::Paths));

    BeginArray(std::string("gamedirs"));
    for (size_t i = 0; i < AndroidSettings::values.game_dirs.size(); ++i) {
        SetArrayIndex(i);
        const auto& game_dir = AndroidSettings::values.game_dirs[i];
        WriteStringSetting(std::string("path"), game_dir.path);
        WriteBooleanSetting(std::string("deep_scan"), game_dir.deep_scan,
                            std::make_optional(false));
    }
    EndArray();

    EndGroup();
}

void AndroidConfig::SaveDriverValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::GpuDriver));

    WriteCategory(Settings::Category::GpuDriver);

    EndGroup();
}

std::vector<Settings::BasicSetting*>& AndroidConfig::FindRelevantList(Settings::Category category) {
    auto& map = Settings::values.linkage.by_category;
    if (map.contains(category)) {
        return Settings::values.linkage.by_category[category];
    }
    return AndroidSettings::values.linkage.by_category[category];
}
