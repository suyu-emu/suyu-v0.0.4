// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include <utility>
#include <QColorDialog>
#include <QGridLayout>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QTimer>
#include "common/assert.h"
#include "common/param_package.h"
#include "input_common/main.h"
#include "ui_configure_input_player.h"
#include "yuzu/configuration/config.h"
#include "yuzu/configuration/configure_input_player.h"

const std::array<std::string, ConfigureInputPlayer::ANALOG_SUB_BUTTONS_NUM>
    ConfigureInputPlayer::analog_sub_buttons{{
        "up",
        "down",
        "left",
        "right",
        "modifier",
    }};

static void LayerGridElements(QGridLayout* grid, QWidget* item, QWidget* onTopOf) {
    const int index1 = grid->indexOf(item);
    const int index2 = grid->indexOf(onTopOf);
    int row, column, rowSpan, columnSpan;
    grid->getItemPosition(index2, &row, &column, &rowSpan, &columnSpan);
    grid->takeAt(index1);
    grid->addWidget(item, row, column, rowSpan, columnSpan);
}

static QString GetKeyName(int key_code) {
    switch (key_code) {
    case Qt::Key_Shift:
        return QObject::tr("Shift");
    case Qt::Key_Control:
        return QObject::tr("Ctrl");
    case Qt::Key_Alt:
        return QObject::tr("Alt");
    case Qt::Key_Meta:
        return {};
    default:
        return QKeySequence(key_code).toString();
    }
}

static void SetAnalogButton(const Common::ParamPackage& input_param,
                            Common::ParamPackage& analog_param, const std::string& button_name) {
    if (analog_param.Get("engine", "") != "analog_from_button") {
        analog_param = {
            {"engine", "analog_from_button"},
            {"modifier_scale", "0.5"},
        };
    }
    analog_param.Set(button_name, input_param.Serialize());
}

static QString ButtonToText(const Common::ParamPackage& param) {
    if (!param.Has("engine")) {
        return QObject::tr("[not set]");
    }

    if (param.Get("engine", "") == "keyboard") {
        return GetKeyName(param.Get("code", 0));
    }

    if (param.Get("engine", "") == "sdl") {
        if (param.Has("hat")) {
            const QString hat_str = QString::fromStdString(param.Get("hat", ""));
            const QString direction_str = QString::fromStdString(param.Get("direction", ""));

            return QObject::tr("Hat %1 %2").arg(hat_str, direction_str);
        }

        if (param.Has("axis")) {
            const QString axis_str = QString::fromStdString(param.Get("axis", ""));
            const QString direction_str = QString::fromStdString(param.Get("direction", ""));

            return QObject::tr("Axis %1%2").arg(axis_str, direction_str);
        }

        if (param.Has("button")) {
            const QString button_str = QString::fromStdString(param.Get("button", ""));

            return QObject::tr("Button %1").arg(button_str);
        }

        return {};
    }

    return QObject::tr("[unknown]");
}

static QString AnalogToText(const Common::ParamPackage& param, const std::string& dir) {
    if (!param.Has("engine")) {
        return QObject::tr("[not set]");
    }

    if (param.Get("engine", "") == "analog_from_button") {
        return ButtonToText(Common::ParamPackage{param.Get(dir, "")});
    }

    if (param.Get("engine", "") == "sdl") {
        if (dir == "modifier") {
            return QObject::tr("[unused]");
        }

        if (dir == "left" || dir == "right") {
            const QString axis_x_str = QString::fromStdString(param.Get("axis_x", ""));

            return QObject::tr("Axis %1").arg(axis_x_str);
        }

        if (dir == "up" || dir == "down") {
            const QString axis_y_str = QString::fromStdString(param.Get("axis_y", ""));

            return QObject::tr("Axis %1").arg(axis_y_str);
        }

        return {};
    }

    return QObject::tr("[unknown]");
}

ConfigureInputPlayer::ConfigureInputPlayer(QWidget* parent, std::size_t player_index, bool debug)
    : QDialog(parent), ui(std::make_unique<Ui::ConfigureInputPlayer>()), player_index(player_index),
      debug(debug), timeout_timer(std::make_unique<QTimer>()),
      poll_timer(std::make_unique<QTimer>()) {
    ui->setupUi(this);
    setFocusPolicy(Qt::ClickFocus);

    button_map = {
        ui->buttonA,          ui->buttonB,        ui->buttonX,           ui->buttonY,
        ui->buttonLStick,     ui->buttonRStick,   ui->buttonL,           ui->buttonR,
        ui->buttonZL,         ui->buttonZR,       ui->buttonPlus,        ui->buttonMinus,
        ui->buttonDpadLeft,   ui->buttonDpadUp,   ui->buttonDpadRight,   ui->buttonDpadDown,
        ui->buttonLStickLeft, ui->buttonLStickUp, ui->buttonLStickRight, ui->buttonLStickDown,
        ui->buttonRStickLeft, ui->buttonRStickUp, ui->buttonRStickRight, ui->buttonRStickDown,
        ui->buttonSL,         ui->buttonSR,       ui->buttonHome,        ui->buttonScreenshot,
    };

    analog_map_buttons = {{
        {
            ui->buttonLStickUp,
            ui->buttonLStickDown,
            ui->buttonLStickLeft,
            ui->buttonLStickRight,
            ui->buttonLStickMod,
        },
        {
            ui->buttonRStickUp,
            ui->buttonRStickDown,
            ui->buttonRStickLeft,
            ui->buttonRStickRight,
            ui->buttonRStickMod,
        },
    }};

    debug_hidden = {
        ui->buttonSL,         ui->labelSL,
        ui->buttonSR,         ui->labelSR,
        ui->buttonLStick,     ui->labelLStickPressed,
        ui->buttonRStick,     ui->labelRStickPressed,
        ui->buttonHome,       ui->labelHome,
        ui->buttonScreenshot, ui->labelScreenshot,
    };

    auto layout = Settings::values.players[player_index].type;
    if (debug)
        layout = Settings::ControllerType::DualJoycon;

    switch (layout) {
    case Settings::ControllerType::ProController:
    case Settings::ControllerType::DualJoycon:
        layout_hidden = {
            ui->buttonSL,
            ui->labelSL,
            ui->buttonSR,
            ui->labelSR,
        };
        break;
    case Settings::ControllerType::LeftJoycon:
        layout_hidden = {
            ui->right_body_button,
            ui->right_buttons_button,
            ui->right_body_label,
            ui->right_buttons_label,
            ui->buttonR,
            ui->labelR,
            ui->buttonZR,
            ui->labelZR,
            ui->labelHome,
            ui->buttonHome,
            ui->buttonPlus,
            ui->labelPlus,
            ui->RStick,
            ui->faceButtons,
        };
        break;
    case Settings::ControllerType::RightJoycon:
        layout_hidden = {
            ui->left_body_button, ui->left_buttons_button,
            ui->left_body_label,  ui->left_buttons_label,
            ui->buttonL,          ui->labelL,
            ui->buttonZL,         ui->labelZL,
            ui->labelScreenshot,  ui->buttonScreenshot,
            ui->buttonMinus,      ui->labelMinus,
            ui->LStick,           ui->Dpad,
        };
        break;
    }

    if (debug || layout == Settings::ControllerType::ProController) {
        ui->controller_color->hide();
    } else {
        if (layout == Settings::ControllerType::LeftJoycon ||
            layout == Settings::ControllerType::RightJoycon) {
            ui->horizontalSpacer_4->setGeometry({0, 0, 0, 0});

            LayerGridElements(ui->buttons, ui->shoulderButtons, ui->Dpad);
            LayerGridElements(ui->buttons, ui->misc, ui->RStick);
            LayerGridElements(ui->buttons, ui->Dpad, ui->faceButtons);
            LayerGridElements(ui->buttons, ui->RStick, ui->LStick);
        }
    }

    for (auto* widget : layout_hidden)
        widget->setVisible(false);

    analog_map_stick = {ui->buttonLStickAnalog, ui->buttonRStickAnalog};
    analog_map_deadzone = {ui->sliderLStickDeadzone, ui->sliderRStickDeadzone};
    analog_map_deadzone_label = {ui->labelLStickDeadzone, ui->labelRStickDeadzone};

    for (int button_id = 0; button_id < Settings::NativeButton::NumButtons; button_id++) {
        auto* const button = button_map[button_id];
        if (button == nullptr) {
            continue;
        }

        button->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(button, &QPushButton::clicked, [=] {
            HandleClick(button_map[button_id],
                        [=](Common::ParamPackage params) {
                            // Workaround for ZL & ZR for analog triggers like on XBOX controllors.
                            // Analog triggers (from controllers like the XBOX controller) would not
                            // work due to a different range of their signals (from 0 to 255 on
                            // analog triggers instead of -32768 to 32768 on analog joysticks). The
                            // SDL driver misinterprets analog triggers as analog joysticks.
                            // TODO: reinterpret the signal range for analog triggers to map the
                            // values correctly. This is required for the correct emulation of the
                            // analog triggers of the GameCube controller.
                            if (button_id == Settings::NativeButton::ZL ||
                                button_id == Settings::NativeButton::ZR) {
                                params.Set("direction", "+");
                                params.Set("threshold", "0.5");
                            }
                            buttons_param[button_id] = std::move(params);
                        },
                        InputCommon::Polling::DeviceType::Button);
        });
        connect(button, &QPushButton::customContextMenuRequested, [=](const QPoint& menu_location) {
            QMenu context_menu;
            context_menu.addAction(tr("Clear"), [&] {
                buttons_param[button_id].Clear();
                button_map[button_id]->setText(tr("[not set]"));
            });
            context_menu.addAction(tr("Restore Default"), [&] {
                buttons_param[button_id] = Common::ParamPackage{
                    InputCommon::GenerateKeyboardParam(Config::default_buttons[button_id])};
                button_map[button_id]->setText(ButtonToText(buttons_param[button_id]));
            });
            context_menu.exec(button_map[button_id]->mapToGlobal(menu_location));
        });
    }

    for (int analog_id = 0; analog_id < Settings::NativeAnalog::NumAnalogs; analog_id++) {
        for (int sub_button_id = 0; sub_button_id < ANALOG_SUB_BUTTONS_NUM; sub_button_id++) {
            auto* const analog_button = analog_map_buttons[analog_id][sub_button_id];
            if (analog_button == nullptr) {
                continue;
            }

            analog_button->setContextMenuPolicy(Qt::CustomContextMenu);
            connect(analog_button, &QPushButton::clicked, [=]() {
                HandleClick(analog_map_buttons[analog_id][sub_button_id],
                            [=](const Common::ParamPackage& params) {
                                SetAnalogButton(params, analogs_param[analog_id],
                                                analog_sub_buttons[sub_button_id]);
                            },
                            InputCommon::Polling::DeviceType::Button);
            });
            connect(analog_button, &QPushButton::customContextMenuRequested,
                    [=](const QPoint& menu_location) {
                        QMenu context_menu;
                        context_menu.addAction(tr("Clear"), [&] {
                            analogs_param[analog_id].Erase(analog_sub_buttons[sub_button_id]);
                            analog_map_buttons[analog_id][sub_button_id]->setText(tr("[not set]"));
                        });
                        context_menu.addAction(tr("Restore Default"), [&] {
                            Common::ParamPackage params{InputCommon::GenerateKeyboardParam(
                                Config::default_analogs[analog_id][sub_button_id])};
                            SetAnalogButton(params, analogs_param[analog_id],
                                            analog_sub_buttons[sub_button_id]);
                            analog_map_buttons[analog_id][sub_button_id]->setText(AnalogToText(
                                analogs_param[analog_id], analog_sub_buttons[sub_button_id]));
                        });
                        context_menu.exec(analog_map_buttons[analog_id][sub_button_id]->mapToGlobal(
                            menu_location));
                    });
        }
        connect(analog_map_stick[analog_id], &QPushButton::clicked, [=] {
            if (QMessageBox::information(
                    this, tr("Information"),
                    tr("After pressing OK, first move your joystick horizontally, "
                       "and then vertically."),
                    QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Ok) {
                HandleClick(
                    analog_map_stick[analog_id],
                    [=](const Common::ParamPackage& params) { analogs_param[analog_id] = params; },
                    InputCommon::Polling::DeviceType::Analog);
            }
        });
        connect(analog_map_deadzone[analog_id], &QSlider::valueChanged, [=] {
            const float deadzone = analog_map_deadzone[analog_id]->value() / 100.0f;
            analog_map_deadzone_label[analog_id]->setText(tr("Deadzone: %1").arg(deadzone));
            analogs_param[analog_id].Set("deadzone", deadzone);
        });
    }

    connect(ui->buttonClearAll, &QPushButton::clicked, [this] { ClearAll(); });
    connect(ui->buttonRestoreDefaults, &QPushButton::clicked, [this] { RestoreDefaults(); });

    timeout_timer->setSingleShot(true);
    connect(timeout_timer.get(), &QTimer::timeout, [this] { SetPollingResult({}, true); });

    connect(poll_timer.get(), &QTimer::timeout, [this] {
        Common::ParamPackage params;
        for (auto& poller : device_pollers) {
            params = poller->GetNextInput();
            if (params.Has("engine")) {
                SetPollingResult(params, false);
                return;
            }
        }
    });

    controller_color_buttons = {
        ui->left_body_button,
        ui->left_buttons_button,
        ui->right_body_button,
        ui->right_buttons_button,
    };

    for (std::size_t i = 0; i < controller_color_buttons.size(); ++i) {
        connect(controller_color_buttons[i], &QPushButton::clicked, this,
                [this, i] { OnControllerButtonClick(static_cast<int>(i)); });
    }

    LoadConfiguration();
    resize(0, 0);

    // TODO(wwylele): enable this when we actually emulate it
    ui->buttonHome->setEnabled(false);
}

ConfigureInputPlayer::~ConfigureInputPlayer() = default;

void ConfigureInputPlayer::ApplyConfiguration() {
    auto& buttons =
        debug ? Settings::values.debug_pad_buttons : Settings::values.players[player_index].buttons;
    auto& analogs =
        debug ? Settings::values.debug_pad_analogs : Settings::values.players[player_index].analogs;

    std::transform(buttons_param.begin(), buttons_param.end(), buttons.begin(),
                   [](const Common::ParamPackage& param) { return param.Serialize(); });
    std::transform(analogs_param.begin(), analogs_param.end(), analogs.begin(),
                   [](const Common::ParamPackage& param) { return param.Serialize(); });

    if (debug)
        return;

    std::array<u32, 4> colors{};
    std::transform(controller_colors.begin(), controller_colors.end(), colors.begin(),
                   [](QColor color) { return color.rgb(); });

    Settings::values.players[player_index].body_color_left = colors[0];
    Settings::values.players[player_index].button_color_left = colors[1];
    Settings::values.players[player_index].body_color_right = colors[2];
    Settings::values.players[player_index].button_color_right = colors[3];
}

void ConfigureInputPlayer::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QDialog::changeEvent(event);
}

void ConfigureInputPlayer::RetranslateUI() {
    ui->retranslateUi(this);
    UpdateButtonLabels();
}

void ConfigureInputPlayer::OnControllerButtonClick(int i) {
    const QColor new_bg_color = QColorDialog::getColor(controller_colors[i]);
    if (!new_bg_color.isValid())
        return;
    controller_colors[i] = new_bg_color;
    controller_color_buttons[i]->setStyleSheet(
        QStringLiteral("QPushButton { background-color: %1 }").arg(controller_colors[i].name()));
}

void ConfigureInputPlayer::LoadConfiguration() {
    if (debug) {
        std::transform(Settings::values.debug_pad_buttons.begin(),
                       Settings::values.debug_pad_buttons.end(), buttons_param.begin(),
                       [](const std::string& str) { return Common::ParamPackage(str); });
        std::transform(Settings::values.debug_pad_analogs.begin(),
                       Settings::values.debug_pad_analogs.end(), analogs_param.begin(),
                       [](const std::string& str) { return Common::ParamPackage(str); });
    } else {
        std::transform(Settings::values.players[player_index].buttons.begin(),
                       Settings::values.players[player_index].buttons.end(), buttons_param.begin(),
                       [](const std::string& str) { return Common::ParamPackage(str); });
        std::transform(Settings::values.players[player_index].analogs.begin(),
                       Settings::values.players[player_index].analogs.end(), analogs_param.begin(),
                       [](const std::string& str) { return Common::ParamPackage(str); });
    }

    UpdateButtonLabels();

    if (debug)
        return;

    std::array<u32, 4> colors = {
        Settings::values.players[player_index].body_color_left,
        Settings::values.players[player_index].button_color_left,
        Settings::values.players[player_index].body_color_right,
        Settings::values.players[player_index].button_color_right,
    };

    std::transform(colors.begin(), colors.end(), controller_colors.begin(),
                   [](u32 rgb) { return QColor::fromRgb(rgb); });

    for (std::size_t i = 0; i < colors.size(); ++i) {
        controller_color_buttons[i]->setStyleSheet(
            QStringLiteral("QPushButton { background-color: %1 }")
                .arg(controller_colors[i].name()));
    }
}

void ConfigureInputPlayer::RestoreDefaults() {
    for (int button_id = 0; button_id < Settings::NativeButton::NumButtons; button_id++) {
        buttons_param[button_id] = Common::ParamPackage{
            InputCommon::GenerateKeyboardParam(Config::default_buttons[button_id])};
    }

    for (int analog_id = 0; analog_id < Settings::NativeAnalog::NumAnalogs; analog_id++) {
        for (int sub_button_id = 0; sub_button_id < ANALOG_SUB_BUTTONS_NUM; sub_button_id++) {
            Common::ParamPackage params{InputCommon::GenerateKeyboardParam(
                Config::default_analogs[analog_id][sub_button_id])};
            SetAnalogButton(params, analogs_param[analog_id], analog_sub_buttons[sub_button_id]);
        }
    }
    UpdateButtonLabels();
}

void ConfigureInputPlayer::ClearAll() {
    for (int button_id = 0; button_id < Settings::NativeButton::NumButtons; button_id++) {
        const auto* const button = button_map[button_id];
        if (button == nullptr || !button->isEnabled()) {
            continue;
        }

        buttons_param[button_id].Clear();
    }

    for (int analog_id = 0; analog_id < Settings::NativeAnalog::NumAnalogs; analog_id++) {
        for (int sub_button_id = 0; sub_button_id < ANALOG_SUB_BUTTONS_NUM; sub_button_id++) {
            const auto* const analog_button = analog_map_buttons[analog_id][sub_button_id];
            if (analog_button == nullptr || !analog_button->isEnabled()) {
                continue;
            }

            analogs_param[analog_id].Clear();
        }
    }

    UpdateButtonLabels();
}

void ConfigureInputPlayer::UpdateButtonLabels() {
    for (int button = 0; button < Settings::NativeButton::NumButtons; button++) {
        button_map[button]->setText(ButtonToText(buttons_param[button]));
    }

    for (int analog_id = 0; analog_id < Settings::NativeAnalog::NumAnalogs; analog_id++) {
        for (int sub_button_id = 0; sub_button_id < ANALOG_SUB_BUTTONS_NUM; sub_button_id++) {
            auto* const analog_button = analog_map_buttons[analog_id][sub_button_id];

            if (analog_button == nullptr) {
                continue;
            }

            analog_button->setText(
                AnalogToText(analogs_param[analog_id], analog_sub_buttons[sub_button_id]));
        }
        analog_map_stick[analog_id]->setText(tr("Set Analog Stick"));

        auto& param = analogs_param[analog_id];
        auto* const analog_deadzone_slider = analog_map_deadzone[analog_id];
        auto* const analog_deadzone_label = analog_map_deadzone_label[analog_id];

        if (param.Has("engine") && param.Get("engine", "") == "sdl") {
            if (!param.Has("deadzone")) {
                param.Set("deadzone", 0.1f);
            }

            analog_deadzone_slider->setValue(static_cast<int>(param.Get("deadzone", 0.1f) * 100));
            analog_deadzone_slider->setVisible(true);
            analog_deadzone_label->setVisible(true);
        } else {
            analog_deadzone_slider->setVisible(false);
            analog_deadzone_label->setVisible(false);
        }
    }
}

void ConfigureInputPlayer::HandleClick(
    QPushButton* button, std::function<void(const Common::ParamPackage&)> new_input_setter,
    InputCommon::Polling::DeviceType type) {
    button->setText(tr("[press key]"));
    button->setFocus();

    const auto iter = std::find(button_map.begin(), button_map.end(), button);
    ASSERT(iter != button_map.end());
    const auto index = std::distance(button_map.begin(), iter);
    ASSERT(index < Settings::NativeButton::NumButtons && index >= 0);

    input_setter = new_input_setter;

    device_pollers = InputCommon::Polling::GetPollers(type);

    // Keyboard keys can only be used as button devices
    want_keyboard_keys = type == InputCommon::Polling::DeviceType::Button;

    for (auto& poller : device_pollers) {
        poller->Start();
    }

    grabKeyboard();
    grabMouse();
    timeout_timer->start(5000); // Cancel after 5 seconds
    poll_timer->start(200);     // Check for new inputs every 200ms
}

void ConfigureInputPlayer::SetPollingResult(const Common::ParamPackage& params, bool abort) {
    releaseKeyboard();
    releaseMouse();
    timeout_timer->stop();
    poll_timer->stop();
    for (auto& poller : device_pollers) {
        poller->Stop();
    }

    if (!abort) {
        (*input_setter)(params);
    }

    UpdateButtonLabels();
    input_setter = std::nullopt;
}

void ConfigureInputPlayer::keyPressEvent(QKeyEvent* event) {
    if (!input_setter || !event) {
        return;
    }

    if (event->key() != Qt::Key_Escape) {
        if (want_keyboard_keys) {
            SetPollingResult(Common::ParamPackage{InputCommon::GenerateKeyboardParam(event->key())},
                             false);
        } else {
            // Escape key wasn't pressed and we don't want any keyboard keys, so don't stop polling
            return;
        }
    }
    SetPollingResult({}, true);
}
