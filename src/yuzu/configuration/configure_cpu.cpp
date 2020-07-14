// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QComboBox>
#include <QMessageBox>

#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/settings.h"
#include "ui_configure_cpu.h"
#include "yuzu/configuration/configure_cpu.h"

ConfigureCpu::ConfigureCpu(QWidget* parent) : QWidget(parent), ui(new Ui::ConfigureCpu) {
    ui->setupUi(this);

    SetConfiguration();

    connect(ui->accuracy, qOverload<int>(&QComboBox::activated), this,
            &ConfigureCpu::AccuracyUpdated);
}

ConfigureCpu::~ConfigureCpu() = default;

void ConfigureCpu::SetConfiguration() {
    const bool runtime_lock = !Core::System::GetInstance().IsPoweredOn();

    ui->accuracy->setEnabled(runtime_lock);
    ui->accuracy->setCurrentIndex(static_cast<int>(Settings::values.cpu_accuracy));
}

void ConfigureCpu::AccuracyUpdated(int index) {
    if (static_cast<Settings::CPUAccuracy>(index) == Settings::CPUAccuracy::DebugMode) {
        const auto result = QMessageBox::warning(this, tr("Setting CPU to Debug Mode"),
                                                 tr("CPU Debug Mode is only intended for developer "
                                                    "use. Are you sure you want to enable this?"),
                                                 QMessageBox::Yes | QMessageBox::No);
        if (result == QMessageBox::No) {
            ui->accuracy->setCurrentIndex(static_cast<int>(Settings::CPUAccuracy::Accurate));
            return;
        }
    }
}

void ConfigureCpu::ApplyConfiguration() {
    Settings::values.cpu_accuracy =
        static_cast<Settings::CPUAccuracy>(ui->accuracy->currentIndex());
}

void ConfigureCpu::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigureCpu::RetranslateUI() {
    ui->retranslateUi(this);
}
