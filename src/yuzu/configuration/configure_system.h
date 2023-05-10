// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <forward_list>
#include <functional>
#include <memory>

#include <QWidget>
#include "yuzu/configuration/configuration_shared.h"

class QDateTimeEdit;

namespace Core {
class System;
}

namespace Ui {
class ConfigureSystem;
}

class ConfigureSystem : public ConfigurationShared::Tab {
public:
    explicit ConfigureSystem(Core::System& system_,
                             std::shared_ptr<std::forward_list<ConfigurationShared::Tab*>> group,
                             ConfigurationShared::TranslationMap& translations,
                             QWidget* parent = nullptr);
    ~ConfigureSystem() override;

    void ApplyConfiguration() override;
    void SetConfiguration() override;

private:
    void changeEvent(QEvent* event) override;
    void RetranslateUI();

    void Setup();

    std::forward_list<std::function<void(bool)>> apply_funcs{};

    std::unique_ptr<Ui::ConfigureSystem> ui;
    bool enabled = false;

    ConfigurationShared::CheckState use_rng_seed;
    ConfigurationShared::CheckState use_unsafe_extended_memory_layout;

    Core::System& system;
    ConfigurationShared::TranslationMap& translations;

    QCheckBox* rng_seed_checkbox;
    QLineEdit* rng_seed_edit;
    QCheckBox* custom_rtc_checkbox;
    QDateTimeEdit* custom_rtc_edit;
    QComboBox* combo_region;
    QComboBox* combo_language;
};
