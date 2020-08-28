// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>

#include <QKeyEvent>
#include <QWidget>

#include "yuzu/configuration/configure_input_advanced.h"
#include "yuzu/configuration/configure_input_player.h"

#include "ui_configure_input.h"

class QCheckBox;
class QString;
class QTimer;

namespace InputCommon {
class InputSubsystem;
}

namespace Ui {
class ConfigureInput;
}

void OnDockedModeChanged(bool last_state, bool new_state);

class ConfigureInput : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureInput(QWidget* parent = nullptr);
    ~ConfigureInput() override;

    /// Initializes the input dialog with the given input subsystem.
    void Initialize(InputCommon::InputSubsystem* input_subsystem_);

    /// Save all button configurations to settings file.
    void ApplyConfiguration();

    QList<QWidget*> GetSubTabs() const;

private:
    void changeEvent(QEvent* event) override;
    void RetranslateUI();
    void ClearAll();

    void UpdateDockedState(bool is_handheld);
    void UpdateAllInputDevices();

    /// Load configuration settings.
    void LoadConfiguration();
    void LoadPlayerControllerIndices();

    /// Restore all buttons to their default values.
    void RestoreDefaults();

    std::unique_ptr<Ui::ConfigureInput> ui;

    std::array<ConfigureInputPlayer*, 8> player_controllers;
    std::array<QWidget*, 8> player_tabs;
    std::array<QCheckBox*, 8> player_connected;
    ConfigureInputAdvanced* advanced;
};
