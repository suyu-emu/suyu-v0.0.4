// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <memory>
#include <QWidget>
#include "yuzu/configuration/configuration_shared.h"
#include "yuzu/configuration/shared_widget.h"

namespace Core {
class System;
}

class ConfigureDialog;
class HotkeyRegistry;

namespace Ui {
class ConfigureGeneral;
}

class ConfigureGeneral : public ConfigurationShared::Tab {
public:
    explicit ConfigureGeneral(
        const Core::System& system_,
        std::shared_ptr<std::forward_list<ConfigurationShared::Tab*>> group,
        const ConfigurationShared::TranslationMap& translations_,
        const ConfigurationShared::ComboboxTranslationMap& combobox_translations_,
        QWidget* parent = nullptr);
    ~ConfigureGeneral() override;

    void SetResetCallback(std::function<void()> callback);
    void ResetDefaults();
    void ApplyConfiguration() override;
    void SetConfiguration() override;

private:
    void changeEvent(QEvent* event) override;
    void RetranslateUI();

    std::function<void()> reset_callback;

    std::unique_ptr<Ui::ConfigureGeneral> ui;

    std::forward_list<std::function<void(bool)>> apply_funcs{};

    const Core::System& system;
    const ConfigurationShared::TranslationMap& translations;
    const ConfigurationShared::ComboboxTranslationMap& combobox_translations;
};
