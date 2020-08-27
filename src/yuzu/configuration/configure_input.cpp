// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>

#include <QSignalBlocker>
#include <QTimer>

#include "core/core.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/am/applet_ae.h"
#include "core/hle/service/am/applet_oe.h"
#include "core/hle/service/sm/sm.h"
#include "ui_configure_input.h"
#include "ui_configure_input_advanced.h"
#include "ui_configure_input_player.h"
#include "yuzu/configuration/configure_debug_controller.h"
#include "yuzu/configuration/configure_input.h"
#include "yuzu/configuration/configure_input_advanced.h"
#include "yuzu/configuration/configure_input_player.h"
#include "yuzu/configuration/configure_mouse_advanced.h"
#include "yuzu/configuration/configure_touchscreen_advanced.h"

namespace {
template <typename Dialog, typename... Args>
void CallConfigureDialog(ConfigureInput& parent, Args&&... args) {
    Dialog dialog(&parent, std::forward<Args>(args)...);

    const auto res = dialog.exec();
    if (res == QDialog::Accepted) {
        dialog.ApplyConfiguration();
    }
}
} // Anonymous namespace

void OnDockedModeChanged(bool last_state, bool new_state) {
    if (last_state == new_state) {
        return;
    }

    Core::System& system{Core::System::GetInstance()};
    if (!system.IsPoweredOn()) {
        return;
    }
    Service::SM::ServiceManager& sm = system.ServiceManager();

    // Message queue is shared between these services, we just need to signal an operation
    // change to one and it will handle both automatically
    auto applet_oe = sm.GetService<Service::AM::AppletOE>("appletOE");
    auto applet_ae = sm.GetService<Service::AM::AppletAE>("appletAE");
    bool has_signalled = false;

    if (applet_oe != nullptr) {
        applet_oe->GetMessageQueue()->OperationModeChanged();
        has_signalled = true;
    }

    if (applet_ae != nullptr && !has_signalled) {
        applet_ae->GetMessageQueue()->OperationModeChanged();
    }
}

ConfigureInput::ConfigureInput(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureInput>()) {
    ui->setupUi(this);
}

ConfigureInput::~ConfigureInput() = default;

void ConfigureInput::Initialize(InputCommon::InputSubsystem* input_subsystem) {
    player_controllers = {
        new ConfigureInputPlayer(this, 0, ui->consoleInputSettings, input_subsystem),
        new ConfigureInputPlayer(this, 1, ui->consoleInputSettings, input_subsystem),
        new ConfigureInputPlayer(this, 2, ui->consoleInputSettings, input_subsystem),
        new ConfigureInputPlayer(this, 3, ui->consoleInputSettings, input_subsystem),
        new ConfigureInputPlayer(this, 4, ui->consoleInputSettings, input_subsystem),
        new ConfigureInputPlayer(this, 5, ui->consoleInputSettings, input_subsystem),
        new ConfigureInputPlayer(this, 6, ui->consoleInputSettings, input_subsystem),
        new ConfigureInputPlayer(this, 7, ui->consoleInputSettings, input_subsystem),
    };

    player_tabs = {
        ui->tabPlayer1, ui->tabPlayer2, ui->tabPlayer3, ui->tabPlayer4,
        ui->tabPlayer5, ui->tabPlayer6, ui->tabPlayer7, ui->tabPlayer8,
    };

    player_connected = {
        ui->checkboxPlayer1Connected, ui->checkboxPlayer2Connected, ui->checkboxPlayer3Connected,
        ui->checkboxPlayer4Connected, ui->checkboxPlayer5Connected, ui->checkboxPlayer6Connected,
        ui->checkboxPlayer7Connected, ui->checkboxPlayer8Connected,
    };

    for (std::size_t i = 0; i < player_tabs.size(); ++i) {
        player_tabs[i]->setLayout(new QHBoxLayout(player_tabs[i]));
        player_tabs[i]->layout()->addWidget(player_controllers[i]);
        connect(player_controllers[i], &ConfigureInputPlayer::Connected, [&, i](bool is_connected) {
            if (is_connected) {
                for (std::size_t index = 0; index <= i; ++index) {
                    player_connected[index]->setChecked(is_connected);
                }
            } else {
                for (std::size_t index = i; index < player_tabs.size(); ++index) {
                    player_connected[index]->setChecked(is_connected);
                }
            }
        });
        connect(player_controllers[i], &ConfigureInputPlayer::RefreshInputDevices,
                [this] { UpdateAllInputDevices(); });
        connect(player_connected[i], &QCheckBox::stateChanged, [this, i](int state) {
            player_controllers[i]->ConnectPlayer(state == Qt::Checked);
        });
    }
    // Only the first player can choose handheld mode so connect the signal just to player 1
    connect(player_controllers[0], &ConfigureInputPlayer::HandheldStateChanged,
            [this](bool is_handheld) { UpdateDockedState(is_handheld); });

    advanced = new ConfigureInputAdvanced(this);
    ui->tabAdvanced->setLayout(new QHBoxLayout(ui->tabAdvanced));
    ui->tabAdvanced->layout()->addWidget(advanced);
    connect(advanced, &ConfigureInputAdvanced::CallDebugControllerDialog, [this, input_subsystem] {
        CallConfigureDialog<ConfigureDebugController>(*this, input_subsystem);
    });
    connect(advanced, &ConfigureInputAdvanced::CallMouseConfigDialog, [this, input_subsystem] {
        CallConfigureDialog<ConfigureMouseAdvanced>(*this, input_subsystem);
    });
    connect(advanced, &ConfigureInputAdvanced::CallTouchscreenConfigDialog,
            [this] { CallConfigureDialog<ConfigureTouchscreenAdvanced>(*this); });

    connect(ui->buttonClearAll, &QPushButton::clicked, [this] { ClearAll(); });
    connect(ui->buttonRestoreDefaults, &QPushButton::clicked, [this] { RestoreDefaults(); });

    RetranslateUI();
    LoadConfiguration();
}

QList<QWidget*> ConfigureInput::GetSubTabs() const {
    return {
        ui->tabPlayer1, ui->tabPlayer2, ui->tabPlayer3, ui->tabPlayer4,  ui->tabPlayer5,
        ui->tabPlayer6, ui->tabPlayer7, ui->tabPlayer8, ui->tabAdvanced,
    };
}

void ConfigureInput::ApplyConfiguration() {
    for (auto controller : player_controllers) {
        controller->ApplyConfiguration();
    }

    advanced->ApplyConfiguration();

    const bool pre_docked_mode = Settings::values.use_docked_mode;
    Settings::values.use_docked_mode = ui->radioDocked->isChecked();
    OnDockedModeChanged(pre_docked_mode, Settings::values.use_docked_mode);

    Settings::values.vibration_enabled = ui->vibrationGroup->isChecked();
}

void ConfigureInput::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigureInput::RetranslateUI() {
    ui->retranslateUi(this);
}

void ConfigureInput::LoadConfiguration() {
    LoadPlayerControllerIndices();
    UpdateDockedState(Settings::values.players[0].controller_type ==
                      Settings::ControllerType::Handheld);

    ui->vibrationGroup->setChecked(Settings::values.vibration_enabled);
}

void ConfigureInput::LoadPlayerControllerIndices() {
    for (std::size_t i = 0; i < player_connected.size(); ++i) {
        const auto connected = Settings::values.players[i].connected ||
                               (i == 0 && Settings::values.players[8].connected);
        player_connected[i]->setChecked(connected);
    }
}

void ConfigureInput::ClearAll() {
    // We don't have a good way to know what tab is active, but we can find out by getting the
    // parent of the consoleInputSettings
    auto* player_tab = static_cast<ConfigureInputPlayer*>(ui->consoleInputSettings->parent());
    player_tab->ClearAll();
}

void ConfigureInput::RestoreDefaults() {
    // We don't have a good way to know what tab is active, but we can find out by getting the
    // parent of the consoleInputSettings
    auto* player_tab = static_cast<ConfigureInputPlayer*>(ui->consoleInputSettings->parent());
    player_tab->RestoreDefaults();

    ui->radioDocked->setChecked(true);
    ui->radioUndocked->setChecked(false);
    ui->vibrationGroup->setChecked(true);
}

void ConfigureInput::UpdateDockedState(bool is_handheld) {
    // If the controller type is handheld only, disallow changing docked mode
    ui->radioDocked->setEnabled(!is_handheld);
    ui->radioUndocked->setEnabled(!is_handheld);

    ui->radioDocked->setChecked(Settings::values.use_docked_mode);
    ui->radioUndocked->setChecked(!Settings::values.use_docked_mode);

    // If its handheld only, force docked mode off (since you can't play handheld in a dock)
    if (is_handheld) {
        ui->radioUndocked->setChecked(true);
    }
}

void ConfigureInput::UpdateAllInputDevices() {
    for (const auto& player : player_controllers) {
        player->UpdateInputDevices();
    }
}
