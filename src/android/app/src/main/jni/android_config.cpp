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
        ReadOverlayValues();
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

void AndroidConfig::ReadOverlayValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::Overlay));

    ReadCategory(Settings::Category::Overlay);

    AndroidSettings::values.overlay_control_data.clear();
    const int control_data_size = BeginArray("control_data");
    for (int i = 0; i < control_data_size; ++i) {
        SetArrayIndex(i);
        AndroidSettings::OverlayControlData control_data;
        control_data.id = ReadStringSetting(std::string("id"));
        control_data.enabled = ReadBooleanSetting(std::string("enabled"));
        control_data.landscape_position.first =
            ReadDoubleSetting(std::string("landscape\\x_position"));
        control_data.landscape_position.second =
            ReadDoubleSetting(std::string("landscape\\y_position"));
        control_data.portrait_position.first =
            ReadDoubleSetting(std::string("portrait\\x_position"));
        control_data.portrait_position.second =
            ReadDoubleSetting(std::string("portrait\\y_position"));
        control_data.foldable_position.first =
            ReadDoubleSetting(std::string("foldable\\x_position"));
        control_data.foldable_position.second =
            ReadDoubleSetting(std::string("foldable\\y_position"));
        AndroidSettings::values.overlay_control_data.push_back(control_data);
    }
    EndArray();

    EndGroup();
}

void AndroidConfig::SaveAndroidValues() {
    if (global) {
        SaveAndroidUIValues();
        SaveUIValues();
        SaveOverlayValues();
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

void AndroidConfig::SaveOverlayValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::Overlay));

    WriteCategory(Settings::Category::Overlay);

    BeginArray("control_data");
    for (size_t i = 0; i < AndroidSettings::values.overlay_control_data.size(); ++i) {
        SetArrayIndex(i);
        const auto& control_data = AndroidSettings::values.overlay_control_data[i];
        WriteStringSetting(std::string("id"), control_data.id);
        WriteBooleanSetting(std::string("enabled"), control_data.enabled);
        WriteDoubleSetting(std::string("landscape\\x_position"),
                           control_data.landscape_position.first);
        WriteDoubleSetting(std::string("landscape\\y_position"),
                           control_data.landscape_position.second);
        WriteDoubleSetting(std::string("portrait\\x_position"),
                           control_data.portrait_position.first);
        WriteDoubleSetting(std::string("portrait\\y_position"),
                           control_data.portrait_position.second);
        WriteDoubleSetting(std::string("foldable\\x_position"),
                           control_data.foldable_position.first);
        WriteDoubleSetting(std::string("foldable\\y_position"),
                           control_data.foldable_position.second);
    }
    EndArray();

    EndGroup();
}

std::vector<Settings::BasicSetting*>& AndroidConfig::FindRelevantList(Settings::Category category) {
    auto& map = Settings::values.linkage.by_category;
    if (map.contains(category)) {
        return Settings::values.linkage.by_category[category];
    }
    return AndroidSettings::values.linkage.by_category[category];
}
