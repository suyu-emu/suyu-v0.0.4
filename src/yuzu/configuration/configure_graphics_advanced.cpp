// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/core.h"
#include "core/settings.h"
#include "ui_configure_graphics_advanced.h"
#include "yuzu/configuration/configure_graphics_advanced.h"

ConfigureGraphicsAdvanced::ConfigureGraphicsAdvanced(QWidget* parent)
    : QWidget(parent), ui(new Ui::ConfigureGraphicsAdvanced) {

    ui->setupUi(this);

    SetConfiguration();
}

ConfigureGraphicsAdvanced::~ConfigureGraphicsAdvanced() = default;

void ConfigureGraphicsAdvanced::SetConfiguration() {
    const bool runtime_lock = !Core::System::GetInstance().IsPoweredOn();
    ui->use_accurate_gpu_emulation->setChecked(Settings::values.use_accurate_gpu_emulation);
    ui->use_vsync->setEnabled(runtime_lock);
    ui->use_vsync->setChecked(Settings::values.use_vsync);
    ui->force_30fps_mode->setEnabled(runtime_lock);
    ui->force_30fps_mode->setChecked(Settings::values.force_30fps_mode);
    ui->anisotropic_filtering_combobox->setEnabled(runtime_lock);
    ui->anisotropic_filtering_combobox->setCurrentIndex(Settings::values.max_anisotropy);
}

void ConfigureGraphicsAdvanced::ApplyConfiguration() {
    Settings::values.use_accurate_gpu_emulation = ui->use_accurate_gpu_emulation->isChecked();
    Settings::values.use_vsync = ui->use_vsync->isChecked();
    Settings::values.force_30fps_mode = ui->force_30fps_mode->isChecked();
    Settings::values.max_anisotropy = ui->anisotropic_filtering_combobox->currentIndex();
}

void ConfigureGraphicsAdvanced::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigureGraphicsAdvanced::RetranslateUI() {
    ui->retranslateUi(this);
}
