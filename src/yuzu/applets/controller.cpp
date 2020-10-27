// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>

#include "common/assert.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/hle/lock.h"
#include "core/hle/service/hid/controllers/npad.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/sm/sm.h"
#include "ui_controller.h"
#include "yuzu/applets/controller.h"
#include "yuzu/configuration/configure_input_dialog.h"
#include "yuzu/main.h"

namespace {

constexpr std::array<std::array<bool, 4>, 8> led_patterns{{
    {true, false, false, false},
    {true, true, false, false},
    {true, true, true, false},
    {true, true, true, true},
    {true, false, false, true},
    {true, false, true, false},
    {true, false, true, true},
    {false, true, true, false},
}};

void UpdateController(Settings::ControllerType controller_type, std::size_t npad_index,
                      bool connected) {
    Core::System& system{Core::System::GetInstance()};

    if (!system.IsPoweredOn()) {
        return;
    }

    Service::SM::ServiceManager& sm = system.ServiceManager();

    auto& npad =
        sm.GetService<Service::HID::Hid>("hid")
            ->GetAppletResource()
            ->GetController<Service::HID::Controller_NPad>(Service::HID::HidController::NPad);

    npad.UpdateControllerAt(npad.MapSettingsTypeToNPad(controller_type), npad_index, connected);
}

// Returns true if the given controller type is compatible with the given parameters.
bool IsControllerCompatible(Settings::ControllerType controller_type,
                            Core::Frontend::ControllerParameters parameters) {
    switch (controller_type) {
    case Settings::ControllerType::ProController:
        return parameters.allow_pro_controller;
    case Settings::ControllerType::DualJoyconDetached:
        return parameters.allow_dual_joycons;
    case Settings::ControllerType::LeftJoycon:
        return parameters.allow_left_joycon;
    case Settings::ControllerType::RightJoycon:
        return parameters.allow_right_joycon;
    case Settings::ControllerType::Handheld:
        return parameters.enable_single_mode && parameters.allow_handheld;
    default:
        return false;
    }
}

/// Maps the controller type combobox index to Controller Type enum
constexpr Settings::ControllerType GetControllerTypeFromIndex(int index) {
    switch (index) {
    case 0:
    default:
        return Settings::ControllerType::ProController;
    case 1:
        return Settings::ControllerType::DualJoyconDetached;
    case 2:
        return Settings::ControllerType::LeftJoycon;
    case 3:
        return Settings::ControllerType::RightJoycon;
    case 4:
        return Settings::ControllerType::Handheld;
    }
}

/// Maps the Controller Type enum to controller type combobox index
constexpr int GetIndexFromControllerType(Settings::ControllerType type) {
    switch (type) {
    case Settings::ControllerType::ProController:
    default:
        return 0;
    case Settings::ControllerType::DualJoyconDetached:
        return 1;
    case Settings::ControllerType::LeftJoycon:
        return 2;
    case Settings::ControllerType::RightJoycon:
        return 3;
    case Settings::ControllerType::Handheld:
        return 4;
    }
}

} // namespace

QtControllerSelectorDialog::QtControllerSelectorDialog(
    QWidget* parent, Core::Frontend::ControllerParameters parameters_,
    InputCommon::InputSubsystem* input_subsystem_)
    : QDialog(parent), ui(std::make_unique<Ui::QtControllerSelectorDialog>()),
      parameters(std::move(parameters_)), input_subsystem(input_subsystem_) {
    ui->setupUi(this);

    player_widgets = {
        ui->widgetPlayer1, ui->widgetPlayer2, ui->widgetPlayer3, ui->widgetPlayer4,
        ui->widgetPlayer5, ui->widgetPlayer6, ui->widgetPlayer7, ui->widgetPlayer8,
    };

    player_groupboxes = {
        ui->groupPlayer1Connected, ui->groupPlayer2Connected, ui->groupPlayer3Connected,
        ui->groupPlayer4Connected, ui->groupPlayer5Connected, ui->groupPlayer6Connected,
        ui->groupPlayer7Connected, ui->groupPlayer8Connected,
    };

    connected_controller_icons = {
        ui->controllerPlayer1, ui->controllerPlayer2, ui->controllerPlayer3, ui->controllerPlayer4,
        ui->controllerPlayer5, ui->controllerPlayer6, ui->controllerPlayer7, ui->controllerPlayer8,
    };

    led_patterns_boxes = {{
        {ui->checkboxPlayer1LED1, ui->checkboxPlayer1LED2, ui->checkboxPlayer1LED3,
         ui->checkboxPlayer1LED4},
        {ui->checkboxPlayer2LED1, ui->checkboxPlayer2LED2, ui->checkboxPlayer2LED3,
         ui->checkboxPlayer2LED4},
        {ui->checkboxPlayer3LED1, ui->checkboxPlayer3LED2, ui->checkboxPlayer3LED3,
         ui->checkboxPlayer3LED4},
        {ui->checkboxPlayer4LED1, ui->checkboxPlayer4LED2, ui->checkboxPlayer4LED3,
         ui->checkboxPlayer4LED4},
        {ui->checkboxPlayer5LED1, ui->checkboxPlayer5LED2, ui->checkboxPlayer5LED3,
         ui->checkboxPlayer5LED4},
        {ui->checkboxPlayer6LED1, ui->checkboxPlayer6LED2, ui->checkboxPlayer6LED3,
         ui->checkboxPlayer6LED4},
        {ui->checkboxPlayer7LED1, ui->checkboxPlayer7LED2, ui->checkboxPlayer7LED3,
         ui->checkboxPlayer7LED4},
        {ui->checkboxPlayer8LED1, ui->checkboxPlayer8LED2, ui->checkboxPlayer8LED3,
         ui->checkboxPlayer8LED4},
    }};

    explain_text_labels = {
        ui->labelPlayer1Explain, ui->labelPlayer2Explain, ui->labelPlayer3Explain,
        ui->labelPlayer4Explain, ui->labelPlayer5Explain, ui->labelPlayer6Explain,
        ui->labelPlayer7Explain, ui->labelPlayer8Explain,
    };

    emulated_controllers = {
        ui->comboPlayer1Emulated, ui->comboPlayer2Emulated, ui->comboPlayer3Emulated,
        ui->comboPlayer4Emulated, ui->comboPlayer5Emulated, ui->comboPlayer6Emulated,
        ui->comboPlayer7Emulated, ui->comboPlayer8Emulated,
    };

    player_labels = {
        ui->labelPlayer1, ui->labelPlayer2, ui->labelPlayer3, ui->labelPlayer4,
        ui->labelPlayer5, ui->labelPlayer6, ui->labelPlayer7, ui->labelPlayer8,
    };

    connected_controller_labels = {
        ui->labelConnectedPlayer1, ui->labelConnectedPlayer2, ui->labelConnectedPlayer3,
        ui->labelConnectedPlayer4, ui->labelConnectedPlayer5, ui->labelConnectedPlayer6,
        ui->labelConnectedPlayer7, ui->labelConnectedPlayer8,
    };

    connected_controller_checkboxes = {
        ui->checkboxPlayer1Connected, ui->checkboxPlayer2Connected, ui->checkboxPlayer3Connected,
        ui->checkboxPlayer4Connected, ui->checkboxPlayer5Connected, ui->checkboxPlayer6Connected,
        ui->checkboxPlayer7Connected, ui->checkboxPlayer8Connected,
    };

    // Setup/load everything prior to setting up connections.
    // This avoids unintentionally changing the states of elements while loading them in.
    SetSupportedControllers();
    DisableUnsupportedPlayers();
    LoadConfiguration();

    for (std::size_t i = 0; i < NUM_PLAYERS; ++i) {
        SetExplainText(i);
        UpdateControllerIcon(i);
        UpdateLEDPattern(i);
        UpdateBorderColor(i);

        connect(player_groupboxes[i], &QGroupBox::toggled, [this, i](bool checked) {
            if (checked) {
                for (std::size_t index = 0; index <= i; ++index) {
                    connected_controller_checkboxes[index]->setChecked(checked);
                }
            } else {
                for (std::size_t index = i; index < NUM_PLAYERS; ++index) {
                    connected_controller_checkboxes[index]->setChecked(checked);
                }
            }
        });

        connect(emulated_controllers[i], qOverload<int>(&QComboBox::currentIndexChanged),
                [this, i](int) {
                    UpdateControllerIcon(i);
                    UpdateControllerState(i);
                    UpdateLEDPattern(i);
                    CheckIfParametersMet();
                });

        connect(connected_controller_checkboxes[i], &QCheckBox::stateChanged, [this, i](int state) {
            player_groupboxes[i]->setChecked(state == Qt::Checked);
            UpdateControllerIcon(i);
            UpdateControllerState(i);
            UpdateLEDPattern(i);
            UpdateBorderColor(i);
            CheckIfParametersMet();
        });

        if (i == 0) {
            connect(emulated_controllers[i], qOverload<int>(&QComboBox::currentIndexChanged),
                    [this](int index) {
                        UpdateDockedState(GetControllerTypeFromIndex(index) ==
                                          Settings::ControllerType::Handheld);
                    });
        }
    }

    connect(ui->inputConfigButton, &QPushButton::clicked, this,
            &QtControllerSelectorDialog::CallConfigureInputDialog);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this,
            &QtControllerSelectorDialog::ApplyConfiguration);

    // If keep_controllers_connected is false, forcefully disconnect all controllers
    if (!parameters.keep_controllers_connected) {
        for (auto player : player_groupboxes) {
            player->setChecked(false);
        }
    }

    CheckIfParametersMet();

    resize(0, 0);
}

QtControllerSelectorDialog::~QtControllerSelectorDialog() = default;

void QtControllerSelectorDialog::ApplyConfiguration() {
    // Update the controller state once more, just to be sure they are properly applied.
    for (std::size_t index = 0; index < NUM_PLAYERS; ++index) {
        UpdateControllerState(index);
    }

    const bool pre_docked_mode = Settings::values.use_docked_mode;
    Settings::values.use_docked_mode = ui->radioDocked->isChecked();
    OnDockedModeChanged(pre_docked_mode, Settings::values.use_docked_mode);

    Settings::values.vibration_enabled = ui->vibrationGroup->isChecked();
}

void QtControllerSelectorDialog::LoadConfiguration() {
    for (std::size_t index = 0; index < NUM_PLAYERS; ++index) {
        const auto connected = Settings::values.players[index].connected ||
                               (index == 0 && Settings::values.players[8].connected);
        player_groupboxes[index]->setChecked(connected);
        connected_controller_checkboxes[index]->setChecked(connected);
        emulated_controllers[index]->setCurrentIndex(
            GetIndexFromControllerType(Settings::values.players[index].controller_type));
    }

    UpdateDockedState(Settings::values.players[8].connected);

    ui->vibrationGroup->setChecked(Settings::values.vibration_enabled);
}

void QtControllerSelectorDialog::CallConfigureInputDialog() {
    const auto max_supported_players = parameters.enable_single_mode ? 1 : parameters.max_players;

    ConfigureInputDialog dialog(this, max_supported_players, input_subsystem);

    dialog.setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint |
                          Qt::WindowSystemMenuHint);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.exec();

    dialog.ApplyConfiguration();

    LoadConfiguration();
    CheckIfParametersMet();
}

void QtControllerSelectorDialog::CheckIfParametersMet() {
    // Here, we check and validate the current configuration against all applicable parameters.
    const auto num_connected_players = static_cast<int>(
        std::count_if(player_groupboxes.begin(), player_groupboxes.end(),
                      [this](const QGroupBox* player) { return player->isChecked(); }));

    const auto min_supported_players = parameters.enable_single_mode ? 1 : parameters.min_players;
    const auto max_supported_players = parameters.enable_single_mode ? 1 : parameters.max_players;

    // First, check against the number of connected players.
    if (num_connected_players < min_supported_players ||
        num_connected_players > max_supported_players) {
        parameters_met = false;
        ui->buttonBox->setEnabled(parameters_met);
        return;
    }

    // Next, check against all connected controllers.
    const auto all_controllers_compatible = [this] {
        for (std::size_t index = 0; index < NUM_PLAYERS; ++index) {
            // Skip controllers that are not used, we only care about the currently connected ones.
            if (!player_groupboxes[index]->isChecked() || !player_groupboxes[index]->isEnabled()) {
                continue;
            }

            const auto compatible = IsControllerCompatible(
                GetControllerTypeFromIndex(emulated_controllers[index]->currentIndex()),
                parameters);

            // If any controller is found to be incompatible, return false early.
            if (!compatible) {
                return false;
            }
        }

        // Reaching here means all currently connected controllers are compatible.
        return true;
    }();

    if (!all_controllers_compatible) {
        parameters_met = false;
        ui->buttonBox->setEnabled(parameters_met);
        return;
    }

    parameters_met = true;
    ui->buttonBox->setEnabled(parameters_met);
}

void QtControllerSelectorDialog::SetSupportedControllers() {
    const QString theme = [this] {
        if (QIcon::themeName().contains(QStringLiteral("dark"))) {
            return QStringLiteral("_dark");
        } else if (QIcon::themeName().contains(QStringLiteral("midnight"))) {
            return QStringLiteral("_midnight");
        } else {
            return QString{};
        }
    }();

    if (parameters.enable_single_mode && parameters.allow_handheld) {
        ui->controllerSupported1->setStyleSheet(
            QStringLiteral("image: url(:/controller/applet_handheld%0); ").arg(theme));
    } else {
        ui->controllerSupported1->setStyleSheet(
            QStringLiteral("image: url(:/controller/applet_handheld%0_disabled); ").arg(theme));
    }

    if (parameters.allow_dual_joycons) {
        ui->controllerSupported2->setStyleSheet(
            QStringLiteral("image: url(:/controller/applet_dual_joycon%0); ").arg(theme));
    } else {
        ui->controllerSupported2->setStyleSheet(
            QStringLiteral("image: url(:/controller/applet_dual_joycon%0_disabled); ").arg(theme));
    }

    if (parameters.allow_left_joycon) {
        ui->controllerSupported3->setStyleSheet(
            QStringLiteral("image: url(:/controller/applet_joycon_left%0); ").arg(theme));
    } else {
        ui->controllerSupported3->setStyleSheet(
            QStringLiteral("image: url(:/controller/applet_joycon_left%0_disabled); ").arg(theme));
    }

    if (parameters.allow_right_joycon) {
        ui->controllerSupported4->setStyleSheet(
            QStringLiteral("image: url(:/controller/applet_joycon_right%0); ").arg(theme));
    } else {
        ui->controllerSupported4->setStyleSheet(
            QStringLiteral("image: url(:/controller/applet_joycon_right%0_disabled); ").arg(theme));
    }

    if (parameters.allow_pro_controller) {
        ui->controllerSupported5->setStyleSheet(
            QStringLiteral("image: url(:/controller/applet_pro_controller%0); ").arg(theme));
    } else {
        ui->controllerSupported5->setStyleSheet(
            QStringLiteral("image: url(:/controller/applet_pro_controller%0_disabled); ")
                .arg(theme));
    }

    // enable_single_mode overrides min_players and max_players.
    if (parameters.enable_single_mode) {
        ui->numberSupportedLabel->setText(QStringLiteral("1"));
        return;
    }

    if (parameters.min_players == parameters.max_players) {
        ui->numberSupportedLabel->setText(QStringLiteral("%1").arg(parameters.max_players));
    } else {
        ui->numberSupportedLabel->setText(
            QStringLiteral("%1 - %2").arg(parameters.min_players).arg(parameters.max_players));
    }
}

void QtControllerSelectorDialog::UpdateControllerIcon(std::size_t player_index) {
    if (!player_groupboxes[player_index]->isChecked()) {
        connected_controller_icons[player_index]->setStyleSheet(QString{});
        player_labels[player_index]->show();
        return;
    }

    const QString stylesheet = [this, player_index] {
        switch (GetControllerTypeFromIndex(emulated_controllers[player_index]->currentIndex())) {
        case Settings::ControllerType::ProController:
            return QStringLiteral("image: url(:/controller/applet_pro_controller%0); ");
        case Settings::ControllerType::DualJoyconDetached:
            return QStringLiteral("image: url(:/controller/applet_dual_joycon%0); ");
        case Settings::ControllerType::LeftJoycon:
            return QStringLiteral("image: url(:/controller/applet_joycon_left%0); ");
        case Settings::ControllerType::RightJoycon:
            return QStringLiteral("image: url(:/controller/applet_joycon_right%0); ");
        case Settings::ControllerType::Handheld:
            return QStringLiteral("image: url(:/controller/applet_handheld%0); ");
        default:
            return QString{};
        }
    }();

    const QString theme = [this] {
        if (QIcon::themeName().contains(QStringLiteral("dark"))) {
            return QStringLiteral("_dark");
        } else if (QIcon::themeName().contains(QStringLiteral("midnight"))) {
            return QStringLiteral("_midnight");
        } else {
            return QString{};
        }
    }();

    connected_controller_icons[player_index]->setStyleSheet(stylesheet.arg(theme));
    player_labels[player_index]->hide();
}

void QtControllerSelectorDialog::UpdateControllerState(std::size_t player_index) {
    auto& player = Settings::values.players[player_index];

    player.controller_type =
        GetControllerTypeFromIndex(emulated_controllers[player_index]->currentIndex());
    player.connected = player_groupboxes[player_index]->isChecked();

    // Player 2-8
    if (player_index != 0) {
        UpdateController(player.controller_type, player_index, player.connected);
        return;
    }

    // Player 1 and Handheld
    auto& handheld = Settings::values.players[8];
    // If Handheld is selected, copy all the settings from Player 1 to Handheld.
    if (player.controller_type == Settings::ControllerType::Handheld) {
        handheld = player;
        handheld.connected = player_groupboxes[player_index]->isChecked();
        player.connected = false; // Disconnect Player 1
    } else {
        player.connected = player_groupboxes[player_index]->isChecked();
        handheld.connected = false; // Disconnect Handheld
    }

    UpdateController(player.controller_type, player_index, player.connected);
    UpdateController(Settings::ControllerType::Handheld, 8, handheld.connected);
}

void QtControllerSelectorDialog::UpdateLEDPattern(std::size_t player_index) {
    if (!player_groupboxes[player_index]->isChecked() ||
        GetControllerTypeFromIndex(emulated_controllers[player_index]->currentIndex()) ==
            Settings::ControllerType::Handheld) {
        led_patterns_boxes[player_index][0]->setChecked(false);
        led_patterns_boxes[player_index][1]->setChecked(false);
        led_patterns_boxes[player_index][2]->setChecked(false);
        led_patterns_boxes[player_index][3]->setChecked(false);
        return;
    }

    led_patterns_boxes[player_index][0]->setChecked(led_patterns[player_index][0]);
    led_patterns_boxes[player_index][1]->setChecked(led_patterns[player_index][1]);
    led_patterns_boxes[player_index][2]->setChecked(led_patterns[player_index][2]);
    led_patterns_boxes[player_index][3]->setChecked(led_patterns[player_index][3]);
}

void QtControllerSelectorDialog::UpdateBorderColor(std::size_t player_index) {
    if (!parameters.enable_border_color ||
        player_index >= static_cast<std::size_t>(parameters.max_players) ||
        player_groupboxes[player_index]->styleSheet().contains(QStringLiteral("QGroupBox"))) {
        return;
    }

    player_groupboxes[player_index]->setStyleSheet(
        player_groupboxes[player_index]->styleSheet().append(
            QStringLiteral("QGroupBox#groupPlayer%1Connected:checked "
                           "{ border: 1px solid rgba(%2, %3, %4, %5); }")
                .arg(player_index + 1)
                .arg(parameters.border_colors[player_index][0])
                .arg(parameters.border_colors[player_index][1])
                .arg(parameters.border_colors[player_index][2])
                .arg(parameters.border_colors[player_index][3])));
}

void QtControllerSelectorDialog::SetExplainText(std::size_t player_index) {
    if (!parameters.enable_explain_text ||
        player_index >= static_cast<std::size_t>(parameters.max_players)) {
        return;
    }

    explain_text_labels[player_index]->setText(QString::fromStdString(
        Common::StringFromFixedZeroTerminatedBuffer(parameters.explain_text[player_index].data(),
                                                    parameters.explain_text[player_index].size())));
}

void QtControllerSelectorDialog::UpdateDockedState(bool is_handheld) {
    // Disallow changing the console mode if the controller type is handheld.
    ui->radioDocked->setEnabled(!is_handheld);
    ui->radioUndocked->setEnabled(!is_handheld);

    ui->radioDocked->setChecked(Settings::values.use_docked_mode);
    ui->radioUndocked->setChecked(!Settings::values.use_docked_mode);

    // Also force into undocked mode if the controller type is handheld.
    if (is_handheld) {
        ui->radioUndocked->setChecked(true);
    }
}

void QtControllerSelectorDialog::DisableUnsupportedPlayers() {
    const auto max_supported_players = parameters.enable_single_mode ? 1 : parameters.max_players;

    switch (max_supported_players) {
    case 0:
    default:
        UNREACHABLE();
        return;
    case 1:
        ui->widgetSpacer->hide();
        ui->widgetSpacer2->hide();
        ui->widgetSpacer3->hide();
        ui->widgetSpacer4->hide();
        break;
    case 2:
        ui->widgetSpacer->hide();
        ui->widgetSpacer2->hide();
        ui->widgetSpacer3->hide();
        break;
    case 3:
        ui->widgetSpacer->hide();
        ui->widgetSpacer2->hide();
        break;
    case 4:
        ui->widgetSpacer->hide();
        break;
    case 5:
    case 6:
    case 7:
    case 8:
        break;
    }

    for (std::size_t index = max_supported_players; index < NUM_PLAYERS; ++index) {
        // Disconnect any unsupported players here and disable or hide them if applicable.
        Settings::values.players[index].connected = false;
        UpdateController(Settings::values.players[index].controller_type, index, false);
        // Hide the player widgets when max_supported_controllers is less than or equal to 4.
        if (max_supported_players <= 4) {
            player_widgets[index]->hide();
        }

        // Disable and hide the following to prevent these from interaction.
        player_widgets[index]->setDisabled(true);
        connected_controller_checkboxes[index]->setDisabled(true);
        connected_controller_labels[index]->hide();
        connected_controller_checkboxes[index]->hide();
    }
}

QtControllerSelector::QtControllerSelector(GMainWindow& parent) {
    connect(this, &QtControllerSelector::MainWindowReconfigureControllers, &parent,
            &GMainWindow::ControllerSelectorReconfigureControllers, Qt::QueuedConnection);
    connect(&parent, &GMainWindow::ControllerSelectorReconfigureFinished, this,
            &QtControllerSelector::MainWindowReconfigureFinished, Qt::QueuedConnection);
}

QtControllerSelector::~QtControllerSelector() = default;

void QtControllerSelector::ReconfigureControllers(
    std::function<void()> callback, const Core::Frontend::ControllerParameters& parameters) const {
    this->callback = std::move(callback);
    emit MainWindowReconfigureControllers(parameters);
}

void QtControllerSelector::MainWindowReconfigureFinished() {
    // Acquire the HLE mutex
    std::lock_guard lock(HLE::g_hle_lock);
    callback();
}
