// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QWidget>
#include "yuzu/configuration/configuration_shared.h"

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
                             QWidget* parent = nullptr);
    ~ConfigureSystem() override;

    void ApplyConfiguration() override;
    void SetConfiguration() override;

private:
    void changeEvent(QEvent* event) override;
    void RetranslateUI();

    void ReadSystemSettings();

    void SetupPerGameUI();

    std::unique_ptr<Ui::ConfigureSystem> ui;
    bool enabled = false;

    ConfigurationShared::CheckState use_rng_seed;
    ConfigurationShared::CheckState use_unsafe_extended_memory_layout;

    Core::System& system;
};
