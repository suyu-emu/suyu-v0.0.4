// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <QWidget>
#include "yuzu/configuration/configuration_shared.h"

namespace Core {
class System;
}

namespace Ui {
class ConfigureGraphicsAdvanced;
}

class ConfigureGraphicsAdvanced : public ConfigurationShared::Tab {
public:
    explicit ConfigureGraphicsAdvanced(
        const Core::System& system_,
        std::shared_ptr<std::forward_list<ConfigurationShared::Tab*>> group,
        const ConfigurationShared::TranslationMap& translations_, QWidget* parent = nullptr);
    ~ConfigureGraphicsAdvanced() override;

    void ApplyConfiguration() override;
    void SetConfiguration() override;

    void ExposeComputeOption();

private:
    void changeEvent(QEvent* event) override;
    void RetranslateUI();

    std::unique_ptr<Ui::ConfigureGraphicsAdvanced> ui;

    std::list<ConfigurationShared::CheckState> trackers{};

    const Core::System& system;
    const ConfigurationShared::TranslationMap& translations;
    std::forward_list<std::function<void(bool)>> apply_funcs;

    QWidget* checkbox_enable_compute_pipelines{};
};
