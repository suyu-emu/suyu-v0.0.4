// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QString>
#include "core/settings.h"

namespace ConfigurationShared {

constexpr int USE_GLOBAL_INDEX = 0;
constexpr int USE_GLOBAL_SEPARATOR_INDEX = 1;
constexpr int USE_GLOBAL_OFFSET = 2;

// Global-aware apply and set functions

void ApplyPerGameSetting(Settings::Setting<bool>* setting, const QCheckBox* checkbox);
void ApplyPerGameSetting(Settings::Setting<int>* setting, const QComboBox* combobox);
void ApplyPerGameSetting(Settings::Setting<Settings::RendererBackend>* setting,
                         const QComboBox* combobox);
void ApplyPerGameSetting(Settings::Setting<Settings::GPUAccuracy>* setting,
                         const QComboBox* combobox);

void SetPerGameSetting(QCheckBox* checkbox, const Settings::Setting<bool>* setting);
void SetPerGameSetting(QComboBox* combobox, const Settings::Setting<int>* setting);
void SetPerGameSetting(QComboBox* combobox,
                       const Settings::Setting<Settings::RendererBackend>* setting);
void SetPerGameSetting(QComboBox* combobox,
                       const Settings::Setting<Settings::GPUAccuracy>* setting);

void InsertGlobalItem(QComboBox* combobox);

} // namespace ConfigurationShared
