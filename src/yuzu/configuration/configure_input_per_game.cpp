// SPDX-FileCopyrightText: 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/settings.h"
#include "core/core.h"
#include "core/hid/emulated_controller.h"
#include "core/hid/hid_core.h"
#include "ui_configure_input_per_game.h"
#include "yuzu/configuration/configure_input_per_game.h"
#include "yuzu/configuration/input_profiles.h"

ConfigureInputPerGame::ConfigureInputPerGame(Core::System& system_, QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureInputPerGame>()),
      profiles(std::make_unique<InputProfiles>()), system{system_} {
    ui->setupUi(this);

    Settings::values.players.SetGlobal(false);
    const auto previous_profile = Settings::values.players.GetValue()[0].profile_name;

    const auto& profile_names = profiles->GetInputProfileNames();

    ui->profile_player_1->addItem(QString::fromStdString("Use global configuration"));
    for (size_t index = 0; index < profile_names.size(); ++index) {
        const auto& profile_name = profile_names[index];
        ui->profile_player_1->addItem(QString::fromStdString(profile_name));
        if (profile_name == previous_profile) {
            // offset by 1 since the first element is the global config
            ui->profile_player_1->setCurrentIndex(static_cast<int>(index + 1));
        }
    }
    LoadConfiguration();
}

void ConfigureInputPerGame::ApplyConfiguration() {
    LoadConfiguration();

    auto& hid_core = system.HIDCore();
    auto* emulated_controller = hid_core.GetEmulatedControllerByIndex(0);

    const auto selection_index = ui->profile_player_1->currentIndex();
    if (selection_index == 0) {
        Settings::values.players.SetGlobal(true);
        emulated_controller->ReloadFromSettings();
        return;
    } else {
        Settings::values.players.SetGlobal(false);
    }
    const QString profile_name = ui->profile_player_1->itemText(selection_index);
    if (profile_name.isEmpty()) {
        return;
    }
    profiles->SaveProfile(Settings::values.players.GetValue()[0].profile_name, 0);
    emulated_controller->ReloadFromSettings();
}

void ConfigureInputPerGame::LoadConfiguration() {
    auto& hid_core = system.HIDCore();
    auto* emulated_controller = hid_core.GetEmulatedControllerByIndex(0);

    Settings::values.players.SetGlobal(false);

    const auto selection_index = ui->profile_player_1->currentIndex();
    if (selection_index == 0) {
        Settings::values.players.GetValue()[0].profile_name = "";
        Settings::values.players.SetGlobal(true);
        emulated_controller->ReloadFromSettings();
        return;
    }
    const QString profile_name = ui->profile_player_1->itemText(selection_index);
    if (profile_name.isEmpty()) {
        return;
    }
    profiles->LoadProfile(profile_name.toStdString(), 0);
    Settings::values.players.GetValue()[0].profile_name = profile_name.toStdString();
    emulated_controller->ReloadFromSettings();
}
