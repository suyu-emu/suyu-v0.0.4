// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include <utility>
#include <QMenu>
#include <QMessageBox>
#include <QTimer>
#include "common/param_package.h"
#include "configuration/configure_touchscreen_advanced.h"
#include "core/core.h"
#include "input_common/main.h"
#include "ui_configure_input.h"
#include "ui_configure_input_player.h"
#include "ui_configure_mouse_advanced.h"
#include "ui_configure_touchscreen_advanced.h"
#include "yuzu/configuration/config.h"
#include "yuzu/configuration/configure_input.h"
#include "yuzu/configuration/configure_input_player.h"
#include "yuzu/configuration/configure_mouse_advanced.h"

ConfigureInput::ConfigureInput(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureInput>()) {
    ui->setupUi(this);

    players_enabled = {
        ui->player1_checkbox, ui->player2_checkbox, ui->player3_checkbox, ui->player4_checkbox,
        ui->player5_checkbox, ui->player6_checkbox, ui->player7_checkbox, ui->player8_checkbox,
    };

    player_controller = {
        ui->player1_combobox, ui->player2_combobox, ui->player3_combobox, ui->player4_combobox,
        ui->player5_combobox, ui->player6_combobox, ui->player7_combobox, ui->player8_combobox,
    };

    player_configure = {
        ui->player1_configure, ui->player2_configure, ui->player3_configure, ui->player4_configure,
        ui->player5_configure, ui->player6_configure, ui->player7_configure, ui->player8_configure,
    };

    for (auto* controller_box : player_controller) {
        controller_box->addItems(
            {"Pro Controller", "Dual Joycons", "Single Right Joycon", "Single Left Joycon"});
    }

    this->loadConfiguration();
    updateUIEnabled();

    connect(ui->restore_defaults_button, &QPushButton::pressed, this,
            &ConfigureInput::restoreDefaults);

    for (auto* enabled : players_enabled)
        connect(enabled, &QCheckBox::stateChanged, this, &ConfigureInput::updateUIEnabled);
    connect(ui->use_docked_mode, &QCheckBox::stateChanged, this, &ConfigureInput::updateUIEnabled);
    connect(ui->handheld_connected, &QCheckBox::stateChanged, this,
            &ConfigureInput::updateUIEnabled);
    connect(ui->mouse_enabled, &QCheckBox::stateChanged, this, &ConfigureInput::updateUIEnabled);
    connect(ui->keyboard_enabled, &QCheckBox::stateChanged, this, &ConfigureInput::updateUIEnabled);
    connect(ui->debug_enabled, &QCheckBox::stateChanged, this, &ConfigureInput::updateUIEnabled);
    connect(ui->touchscreen_enabled, &QCheckBox::stateChanged, this,
            &ConfigureInput::updateUIEnabled);

    for (std::size_t i = 0; i < player_configure.size(); ++i) {
        connect(player_configure[i], &QPushButton::pressed, this,
                [this, i]() { CallConfigureDialog<ConfigureInputPlayer>(i, false); });
    }

    connect(ui->handheld_configure, &QPushButton::pressed, this,
            [this]() { CallConfigureDialog<ConfigureInputPlayer>(8, false); });

    connect(ui->debug_configure, &QPushButton::pressed, this,
            [this]() { CallConfigureDialog<ConfigureInputPlayer>(9, true); });

    connect(ui->mouse_advanced, &QPushButton::pressed, this,
            [this]() { CallConfigureDialog<ConfigureMouseAdvanced>(); });

    connect(ui->touchscreen_advanced, &QPushButton::pressed, this,
            [this]() { CallConfigureDialog<ConfigureTouchscreenAdvanced>(); });

    ui->use_docked_mode->setEnabled(!Core::System::GetInstance().IsPoweredOn());
}

template <typename Dialog, typename... Args>
void ConfigureInput::CallConfigureDialog(Args... args) {
    this->applyConfiguration();
    Dialog dialog(this, args...);

    const auto res = dialog.exec();
    if (res == QDialog::Accepted) {
        dialog.applyConfiguration();
    }
}

void ConfigureInput::applyConfiguration() {
    for (std::size_t i = 0; i < 8; ++i) {
        Settings::values.players[i].connected = players_enabled[i]->isChecked();
        Settings::values.players[i].type =
            static_cast<Settings::ControllerType>(player_controller[i]->currentIndex());
    }

    Settings::values.use_docked_mode = ui->use_docked_mode->isChecked();
    Settings::values.players[8].connected = ui->handheld_connected->isChecked();
    Settings::values.debug_pad_enabled = ui->debug_enabled->isChecked();
    Settings::values.mouse_enabled = ui->mouse_enabled->isChecked();
    Settings::values.keyboard_enabled = ui->keyboard_enabled->isChecked();
    Settings::values.touchscreen.enabled = ui->touchscreen_enabled->isChecked();
}

void ConfigureInput::updateUIEnabled() {
    for (std::size_t i = 0; i < 8; ++i) {
        const auto enabled = players_enabled[i]->checkState() == Qt::Checked;

        player_controller[i]->setEnabled(enabled);
        player_configure[i]->setEnabled(enabled);
    }

    bool hit_disabled = false;
    for (auto* player : players_enabled) {
        if (hit_disabled)
            player->setDisabled(true);
        else
            player->setEnabled(true);
        if (!player->isChecked())
            hit_disabled = true;
    }

    ui->handheld_connected->setEnabled(!ui->use_docked_mode->isChecked());
    ui->handheld_configure->setEnabled(ui->handheld_connected->isChecked() &&
                                       !ui->use_docked_mode->isChecked());
    ui->mouse_advanced->setEnabled(ui->mouse_enabled->isChecked());
    ui->debug_configure->setEnabled(ui->debug_enabled->isChecked());
    ui->touchscreen_advanced->setEnabled(ui->touchscreen_enabled->isChecked());
}

void ConfigureInput::loadConfiguration() {
    std::stable_partition(Settings::values.players.begin(), Settings::values.players.end(),
                          [](const auto& player) { return player.connected; });

    for (std::size_t i = 0; i < 8; ++i) {
        players_enabled[i]->setChecked(Settings::values.players[i].connected);
        player_controller[i]->setCurrentIndex(static_cast<u8>(Settings::values.players[i].type));
    }

    ui->use_docked_mode->setChecked(Settings::values.use_docked_mode);
    ui->handheld_connected->setChecked(Settings::values.players[8].connected);
    ui->debug_enabled->setChecked(Settings::values.debug_pad_enabled);
    ui->mouse_enabled->setChecked(Settings::values.mouse_enabled);
    ui->keyboard_enabled->setChecked(Settings::values.keyboard_enabled);
    ui->touchscreen_enabled->setChecked(Settings::values.touchscreen.enabled);

    updateUIEnabled();
}

void ConfigureInput::restoreDefaults() {
    players_enabled[0]->setCheckState(Qt::Checked);
    player_controller[0]->setCurrentIndex(1);

    for (std::size_t i = 1; i < 8; ++i) {
        players_enabled[i]->setCheckState(Qt::Unchecked);
        player_controller[i]->setCurrentIndex(0);
    }

    ui->use_docked_mode->setCheckState(Qt::Unchecked);
    ui->handheld_connected->setCheckState(Qt::Unchecked);
    ui->mouse_enabled->setCheckState(Qt::Unchecked);
    ui->keyboard_enabled->setCheckState(Qt::Unchecked);
    ui->debug_enabled->setCheckState(Qt::Unchecked);
    ui->touchscreen_enabled->setCheckState(Qt::Checked);
    updateUIEnabled();
}
