// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include <QKeyEvent>
#include <QWidget>

#include "common/param_package.h"
#include "core/settings.h"
#include "input_common/main.h"
#include "ui_configure_input.h"
#include "yuzu/configuration/config.h"

class QPushButton;
class QString;
class QTimer;

namespace Ui {
class ConfigureInput;
}

class ConfigureInput : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureInput(QWidget* parent = nullptr);

    /// Save all button configurations to settings file
    void applyConfiguration();

private:
    void updateUIEnabled();

    template <typename Dialog, typename... Args>
    void CallConfigureDialog(Args... args);

    /// Load configuration settings.
    void loadConfiguration();
    /// Restore all buttons to their default values.
    void restoreDefaults();

    std::unique_ptr<Ui::ConfigureInput> ui;

    std::array<QCheckBox*, 8> players_enabled;
    std::array<QComboBox*, 8> player_controller;
    std::array<QPushButton*, 8> player_configure;
};
