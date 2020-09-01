// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QColorDialog>
#include "core/core.h"
#include "core/settings.h"
#include "ui_configure_input_advanced.h"
#include "yuzu/configuration/configure_input_advanced.h"

ConfigureInputAdvanced::ConfigureInputAdvanced(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureInputAdvanced>()) {
    ui->setupUi(this);

    controllers_color_buttons = {{
        {
            ui->player1_left_body_button,
            ui->player1_left_buttons_button,
            ui->player1_right_body_button,
            ui->player1_right_buttons_button,
        },
        {
            ui->player2_left_body_button,
            ui->player2_left_buttons_button,
            ui->player2_right_body_button,
            ui->player2_right_buttons_button,
        },
        {
            ui->player3_left_body_button,
            ui->player3_left_buttons_button,
            ui->player3_right_body_button,
            ui->player3_right_buttons_button,
        },
        {
            ui->player4_left_body_button,
            ui->player4_left_buttons_button,
            ui->player4_right_body_button,
            ui->player4_right_buttons_button,
        },
        {
            ui->player5_left_body_button,
            ui->player5_left_buttons_button,
            ui->player5_right_body_button,
            ui->player5_right_buttons_button,
        },
        {
            ui->player6_left_body_button,
            ui->player6_left_buttons_button,
            ui->player6_right_body_button,
            ui->player6_right_buttons_button,
        },
        {
            ui->player7_left_body_button,
            ui->player7_left_buttons_button,
            ui->player7_right_body_button,
            ui->player7_right_buttons_button,
        },
        {
            ui->player8_left_body_button,
            ui->player8_left_buttons_button,
            ui->player8_right_body_button,
            ui->player8_right_buttons_button,
        },
    }};

    for (std::size_t player_idx = 0; player_idx < controllers_color_buttons.size(); ++player_idx) {
        auto& color_buttons = controllers_color_buttons[player_idx];
        for (std::size_t button_idx = 0; button_idx < color_buttons.size(); ++button_idx) {
            connect(color_buttons[button_idx], &QPushButton::clicked, this,
                    [this, player_idx, button_idx] {
                        OnControllerButtonClick(static_cast<int>(player_idx),
                                                static_cast<int>(button_idx));
                    });
        }
    }

    connect(ui->mouse_enabled, &QCheckBox::stateChanged, this,
            &ConfigureInputAdvanced::UpdateUIEnabled);
    connect(ui->debug_enabled, &QCheckBox::stateChanged, this,
            &ConfigureInputAdvanced::UpdateUIEnabled);
    connect(ui->touchscreen_enabled, &QCheckBox::stateChanged, this,
            &ConfigureInputAdvanced::UpdateUIEnabled);

    connect(ui->debug_configure, &QPushButton::clicked, this,
            [this] { CallDebugControllerDialog(); });
    connect(ui->mouse_advanced, &QPushButton::clicked, this, [this] { CallMouseConfigDialog(); });
    connect(ui->touchscreen_advanced, &QPushButton::clicked, this,
            [this] { CallTouchscreenConfigDialog(); });
    connect(ui->buttonMotionTouch, &QPushButton::clicked, this,
            &ConfigureInputAdvanced::CallMotionTouchConfigDialog);

    LoadConfiguration();
}

ConfigureInputAdvanced::~ConfigureInputAdvanced() = default;

void ConfigureInputAdvanced::OnControllerButtonClick(int player_idx, int button_idx) {
    const QColor new_bg_color = QColorDialog::getColor(controllers_colors[player_idx][button_idx]);
    if (!new_bg_color.isValid()) {
        return;
    }
    controllers_colors[player_idx][button_idx] = new_bg_color;
    controllers_color_buttons[player_idx][button_idx]->setStyleSheet(
        QStringLiteral("background-color: %1; min-width: 55px;")
            .arg(controllers_colors[player_idx][button_idx].name()));
}

void ConfigureInputAdvanced::ApplyConfiguration() {
    for (std::size_t player_idx = 0; player_idx < controllers_color_buttons.size(); ++player_idx) {
        auto& player = Settings::values.players[player_idx];
        std::array<u32, 4> colors{};
        std::transform(controllers_colors[player_idx].begin(), controllers_colors[player_idx].end(),
                       colors.begin(), [](QColor color) { return color.rgb(); });

        player.body_color_left = colors[0];
        player.button_color_left = colors[1];
        player.body_color_right = colors[2];
        player.button_color_right = colors[3];
    }

    Settings::values.debug_pad_enabled = ui->debug_enabled->isChecked();
    Settings::values.mouse_enabled = ui->mouse_enabled->isChecked();
    Settings::values.keyboard_enabled = ui->keyboard_enabled->isChecked();
    Settings::values.touchscreen.enabled = ui->touchscreen_enabled->isChecked();
}

void ConfigureInputAdvanced::LoadConfiguration() {
    for (std::size_t player_idx = 0; player_idx < controllers_color_buttons.size(); ++player_idx) {
        auto& player = Settings::values.players[player_idx];
        std::array<u32, 4> colors = {
            player.body_color_left,
            player.button_color_left,
            player.body_color_right,
            player.button_color_right,
        };

        std::transform(colors.begin(), colors.end(), controllers_colors[player_idx].begin(),
                       [](u32 rgb) { return QColor::fromRgb(rgb); });

        for (std::size_t button_idx = 0; button_idx < colors.size(); ++button_idx) {
            controllers_color_buttons[player_idx][button_idx]->setStyleSheet(
                QStringLiteral("background-color: %1; min-width: 55px;")
                    .arg(controllers_colors[player_idx][button_idx].name()));
        }
    }

    ui->debug_enabled->setChecked(Settings::values.debug_pad_enabled);
    ui->mouse_enabled->setChecked(Settings::values.mouse_enabled);
    ui->keyboard_enabled->setChecked(Settings::values.keyboard_enabled);
    ui->touchscreen_enabled->setChecked(Settings::values.touchscreen.enabled);

    UpdateUIEnabled();
}

void ConfigureInputAdvanced::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigureInputAdvanced::RetranslateUI() {
    ui->retranslateUi(this);
}

void ConfigureInputAdvanced::UpdateUIEnabled() {
    ui->mouse_advanced->setEnabled(ui->mouse_enabled->isChecked());
    ui->debug_configure->setEnabled(ui->debug_enabled->isChecked());
    ui->touchscreen_advanced->setEnabled(ui->touchscreen_enabled->isChecked());
}
