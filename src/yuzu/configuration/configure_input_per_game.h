// SPDX-FileCopyrightText: 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QWidget>

namespace Core {
class System;
}

namespace InputCommon {
class InputSubsystem;
}

namespace Ui {
class ConfigureInputPerGame;
}

class InputProfiles;

class ConfigureInputPerGame : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureInputPerGame(Core::System& system_, QWidget* parent = nullptr);

    /// Initializes the input dialog with the given input subsystem.
    // void Initialize(InputCommon::InputSubsystem* input_subsystem_, std::size_t max_players = 8);

    /// Save configurations to settings file.
    void ApplyConfiguration();

private:
    /// Load configuration from settings file.
    void LoadConfiguration();

    std::unique_ptr<Ui::ConfigureInputPerGame> ui;
    std::unique_ptr<InputProfiles> profiles;

    Core::System& system;
};
