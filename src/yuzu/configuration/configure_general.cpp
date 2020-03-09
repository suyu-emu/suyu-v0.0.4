// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QCheckBox>
#include <QSpinBox>
#include "core/core.h"
#include "core/settings.h"
#include "ui_configure_general.h"
#include "yuzu/configuration/configure_general.h"
#include "yuzu/uisettings.h"

ConfigureGeneral::ConfigureGeneral(QWidget* parent)
    : QWidget(parent), ui(new Ui::ConfigureGeneral) {

    ui->setupUi(this);

    SetConfiguration();

    connect(ui->toggle_frame_limit, &QCheckBox::toggled, ui->frame_limit, &QSpinBox::setEnabled);
}

ConfigureGeneral::~ConfigureGeneral() = default;

void ConfigureGeneral::SetConfiguration() {
    const bool runtime_lock = !Core::System::GetInstance().IsPoweredOn();

    ui->use_multi_core->setEnabled(runtime_lock);
    ui->use_multi_core->setChecked(Settings::values.use_multi_core);

    ui->toggle_check_exit->setChecked(UISettings::values.confirm_before_closing);
    ui->toggle_user_on_boot->setChecked(UISettings::values.select_user_on_boot);
    ui->toggle_background_pause->setChecked(UISettings::values.pause_when_in_background);
    ui->toggle_hide_mouse->setChecked(UISettings::values.hide_mouse);

    ui->toggle_frame_limit->setChecked(Settings::values.use_frame_limit);
    ui->frame_limit->setEnabled(ui->toggle_frame_limit->isChecked());
    ui->frame_limit->setValue(Settings::values.frame_limit);
}

void ConfigureGeneral::ApplyConfiguration() {
    UISettings::values.confirm_before_closing = ui->toggle_check_exit->isChecked();
    UISettings::values.select_user_on_boot = ui->toggle_user_on_boot->isChecked();
    UISettings::values.pause_when_in_background = ui->toggle_background_pause->isChecked();
    UISettings::values.hide_mouse = ui->toggle_hide_mouse->isChecked();

    Settings::values.use_frame_limit = ui->toggle_frame_limit->isChecked();
    Settings::values.frame_limit = ui->frame_limit->value();
    Settings::values.use_multi_core = ui->use_multi_core->isChecked();
}

void ConfigureGeneral::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigureGeneral::RetranslateUI() {
    ui->retranslateUi(this);
}
