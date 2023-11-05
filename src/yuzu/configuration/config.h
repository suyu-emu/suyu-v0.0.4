// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>
#include <string>
#include <QMetaType>
#include <QVariant>
#include "common/settings.h"
#include "common/settings_enums.h"
#include "yuzu/uisettings.h"

class QSettings;

namespace Core {
class System;
}

class Config {
public:
    enum class ConfigType {
        GlobalConfig,
        PerGameConfig,
        InputProfile,
    };

    explicit Config(const std::string& config_name = "qt-config",
                    ConfigType config_type = ConfigType::GlobalConfig);
    ~Config();

    void Reload();
    void Save();

    void ReadControlPlayerValue(std::size_t player_index);
    void SaveControlPlayerValue(std::size_t player_index);
    void ClearControlPlayerValues();

    const std::string& GetConfigFilePath() const;

    static const std::array<int, Settings::NativeButton::NumButtons> default_buttons;
    static const std::array<int, Settings::NativeMotion::NumMotions> default_motions;
    static const std::array<std::array<int, 4>, Settings::NativeAnalog::NumAnalogs> default_analogs;
    static const std::array<int, 2> default_stick_mod;
    static const std::array<int, 2> default_ringcon_analogs;
    static const std::array<int, Settings::NativeMouseButton::NumMouseButtons>
        default_mouse_buttons;
    static const std::array<int, Settings::NativeKeyboard::NumKeyboardKeys> default_keyboard_keys;
    static const std::array<int, Settings::NativeKeyboard::NumKeyboardMods> default_keyboard_mods;
    static const std::array<UISettings::Shortcut, 23> default_hotkeys;

    static const std::map<Settings::AntiAliasing, QString> anti_aliasing_texts_map;
    static const std::map<Settings::ScalingFilter, QString> scaling_filter_texts_map;
    static const std::map<Settings::ConsoleMode, QString> use_docked_mode_texts_map;
    static const std::map<Settings::GpuAccuracy, QString> gpu_accuracy_texts_map;
    static const std::map<Settings::RendererBackend, QString> renderer_backend_texts_map;
    static const std::map<Settings::ShaderBackend, QString> shader_backend_texts_map;

    static constexpr UISettings::Theme default_theme{
#ifdef _WIN32
        UISettings::Theme::DarkColorful
#else
        UISettings::Theme::DefaultColorful
#endif
    };

private:
    void Initialize(const std::string& config_name);
    bool IsCustomConfig();

    void ReadValues();
    void ReadPlayerValue(std::size_t player_index);
    void ReadDebugValues();
    void ReadKeyboardValues();
    void ReadMouseValues();
    void ReadTouchscreenValues();
    void ReadMotionTouchValues();
    void ReadHidbusValues();
    void ReadIrCameraValues();

    // Read functions bases off the respective config section names.
    void ReadAudioValues();
    void ReadControlValues();
    void ReadCoreValues();
    void ReadDataStorageValues();
    void ReadDebuggingValues();
    void ReadServiceValues();
    void ReadDisabledAddOnValues();
    void ReadMiscellaneousValues();
    void ReadPathValues();
    void ReadCpuValues();
    void ReadRendererValues();
    void ReadScreenshotValues();
    void ReadShortcutValues();
    void ReadSystemValues();
    void ReadUIValues();
    void ReadUIGamelistValues();
    void ReadUILayoutValues();
    void ReadWebServiceValues();
    void ReadMultiplayerValues();
    void ReadNetworkValues();

    void SaveValues();
    void SavePlayerValue(std::size_t player_index);
    void SaveDebugValues();
    void SaveMouseValues();
    void SaveTouchscreenValues();
    void SaveMotionTouchValues();
    void SaveHidbusValues();
    void SaveIrCameraValues();

    // Save functions based off the respective config section names.
    void SaveAudioValues();
    void SaveControlValues();
    void SaveCoreValues();
    void SaveDataStorageValues();
    void SaveDebuggingValues();
    void SaveNetworkValues();
    void SaveDisabledAddOnValues();
    void SaveMiscellaneousValues();
    void SavePathValues();
    void SaveCpuValues();
    void SaveRendererValues();
    void SaveScreenshotValues();
    void SaveShortcutValues();
    void SaveSystemValues();
    void SaveUIValues();
    void SaveUIGamelistValues();
    void SaveUILayoutValues();
    void SaveWebServiceValues();
    void SaveMultiplayerValues();

    /**
     * Reads a setting from the qt_config.
     *
     * @param name The setting's identifier
     * @param default_value The value to use when the setting is not already present in the config
     */
    QVariant ReadSetting(const QString& name) const;
    QVariant ReadSetting(const QString& name, const QVariant& default_value) const;

    /**
     * Writes a setting to the qt_config.
     *
     * @param name The setting's idetentifier
     * @param value Value of the setting
     * @param default_value Default of the setting if not present in qt_config
     * @param use_global Specifies if the custom or global config should be in use, for custom
     * configs
     */
    void WriteSetting(const QString& name, const QVariant& value);
    void WriteSetting(const QString& name, const QVariant& value, const QVariant& default_value);
    void WriteSetting(const QString& name, const QVariant& value, const QVariant& default_value,
                      bool use_global);

    void ReadCategory(Settings::Category category);
    void WriteCategory(Settings::Category category);
    void ReadSettingGeneric(Settings::BasicSetting* const setting);
    void WriteSettingGeneric(Settings::BasicSetting* const setting) const;

    const ConfigType type;
    std::unique_ptr<QSettings> qt_config;
    std::string qt_config_loc;
    const bool global;
};

// These metatype declarations cannot be in common/settings.h because core is devoid of QT
Q_DECLARE_METATYPE(Settings::CpuAccuracy);
Q_DECLARE_METATYPE(Settings::GpuAccuracy);
Q_DECLARE_METATYPE(Settings::FullscreenMode);
Q_DECLARE_METATYPE(Settings::NvdecEmulation);
Q_DECLARE_METATYPE(Settings::ResolutionSetup);
Q_DECLARE_METATYPE(Settings::ScalingFilter);
Q_DECLARE_METATYPE(Settings::AntiAliasing);
Q_DECLARE_METATYPE(Settings::RendererBackend);
Q_DECLARE_METATYPE(Settings::ShaderBackend);
Q_DECLARE_METATYPE(Settings::AstcRecompression);
Q_DECLARE_METATYPE(Settings::AstcDecodeMode);
