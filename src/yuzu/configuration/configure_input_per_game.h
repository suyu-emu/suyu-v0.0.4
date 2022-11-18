// SPDX-FileCopyrightText: 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QWidget>

namespace Core {
class System;
}

namespace Ui {
class ConfigureInputPerGame;
}

class InputProfiles;

class ConfigureInputPerGame : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureInputPerGame(Core::System& system_, QWidget* parent = nullptr);

    /// Load and Save configurations to settings file.
    void ApplyConfiguration();

private:
    /// Load configuration from settings file.
    void LoadConfiguration();

    /// Save configuration to settings file.
    void SaveConfiguration();

    std::unique_ptr<Ui::ConfigureInputPerGame> ui;
    std::unique_ptr<InputProfiles> profiles;

    std::array<QComboBox*, 8> profile_comboboxes;

    Core::System& system;
};
