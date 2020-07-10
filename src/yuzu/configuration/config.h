// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <string>
#include <QMetaType>
#include <QVariant>
#include "core/settings.h"
#include "yuzu/uisettings.h"

class QSettings;

class Config {
public:
    explicit Config(const std::string& config_loc = "qt-config.ini", bool is_global = true);
    ~Config();

    void Reload();
    void Save();

    static const std::array<int, Settings::NativeButton::NumButtons> default_buttons;
    static const std::array<std::array<int, 5>, Settings::NativeAnalog::NumAnalogs> default_analogs;
    static const std::array<int, Settings::NativeMouseButton::NumMouseButtons>
        default_mouse_buttons;
    static const std::array<int, Settings::NativeKeyboard::NumKeyboardKeys> default_keyboard_keys;
    static const std::array<int, Settings::NativeKeyboard::NumKeyboardMods> default_keyboard_mods;
    static const std::array<UISettings::Shortcut, 16> default_hotkeys;

private:
    void ReadValues();
    void ReadPlayerValues();
    void ReadDebugValues();
    void ReadKeyboardValues();
    void ReadMouseValues();
    void ReadTouchscreenValues();
    void ApplyDefaultProfileIfInputInvalid();

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
    void ReadRendererValues();
    void ReadShortcutValues();
    void ReadSystemValues();
    void ReadUIValues();
    void ReadUIGamelistValues();
    void ReadUILayoutValues();
    void ReadWebServiceValues();

    void SaveValues();
    void SavePlayerValues();
    void SaveDebugValues();
    void SaveMouseValues();
    void SaveTouchscreenValues();

    // Save functions based off the respective config section names.
    void SaveAudioValues();
    void SaveControlValues();
    void SaveCoreValues();
    void SaveDataStorageValues();
    void SaveDebuggingValues();
    void SaveServiceValues();
    void SaveDisabledAddOnValues();
    void SaveMiscellaneousValues();
    void SavePathValues();
    void SaveRendererValues();
    void SaveShortcutValues();
    void SaveSystemValues();
    void SaveUIValues();
    void SaveUIGamelistValues();
    void SaveUILayoutValues();
    void SaveWebServiceValues();

    QVariant ReadSetting(const QString& name) const;
    QVariant ReadSetting(const QString& name, const QVariant& default_value) const;
    // Templated ReadSettingGlobal functions will also look for the use_global setting and set
    // both the value and the global state properly
    template <typename Type>
    void ReadSettingGlobal(Settings::Setting<Type>& setting, const QString& name);
    template <typename Type>
    void ReadSettingGlobal(Settings::Setting<Type>& setting, const QString& name,
                           const QVariant& default_value);
    template <typename Type>
    void ReadSettingGlobal(Type& setting, const QString& name, const QVariant& default_value) const;
    // Templated WriteSettingGlobal functions will also write the global state if needed and will
    // skip writing the actual setting if it defers to the global value
    void WriteSetting(const QString& name, const QVariant& value);
    void WriteSetting(const QString& name, const QVariant& value, const QVariant& default_value);
    template <typename Type>
    void WriteSettingGlobal(const QString& name, const Settings::Setting<Type>& setting);
    template <typename Type>
    void WriteSettingGlobal(const QString& name, const Settings::Setting<Type>& setting,
                            const QVariant& default_value);
    void WriteSettingGlobal(const QString& name, const QVariant& value, bool use_global,
                            const QVariant& default_value);

    std::unique_ptr<QSettings> qt_config;
    std::string qt_config_loc;

    bool global;
};

// These metatype declarations cannot be in core/settings.h because core is devoid of QT
Q_DECLARE_METATYPE(Settings::RendererBackend);
Q_DECLARE_METATYPE(Settings::GPUAccuracy);
