// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "yuzu/set_play_time_dialog.h"
#include "frontend_common/play_time_manager.h"
#include "ui_set_play_time_dialog.h"

SetPlayTimeDialog::SetPlayTimeDialog(QWidget* parent, u64 current_play_time)
    : QDialog(parent), ui{std::make_unique<Ui::SetPlayTimeDialog>()} {
    ui->setupUi(this);

    ui->hoursSpinBox->setValue(
        QString::fromStdString(PlayTime::PlayTimeManager::GetPlayTimeHours(current_play_time)).toInt());
    ui->minutesSpinBox->setValue(
        QString::fromStdString(PlayTime::PlayTimeManager::GetPlayTimeMinutes(current_play_time)).toInt());
    ui->secondsSpinBox->setValue(
        QString::fromStdString(PlayTime::PlayTimeManager::GetPlayTimeSeconds(current_play_time)).toInt());

    connect(ui->hoursSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &SetPlayTimeDialog::OnValueChanged);
    connect(ui->minutesSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &SetPlayTimeDialog::OnValueChanged);
    connect(ui->secondsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &SetPlayTimeDialog::OnValueChanged);
}

SetPlayTimeDialog::~SetPlayTimeDialog() = default;

u64 SetPlayTimeDialog::GetTotalSeconds() const {
    const u64 hours = static_cast<u64>(ui->hoursSpinBox->value());
    const u64 minutes = static_cast<u64>(ui->minutesSpinBox->value());
    const u64 seconds = static_cast<u64>(ui->secondsSpinBox->value());

    return hours * 3600 + minutes * 60 + seconds;
}

void SetPlayTimeDialog::OnValueChanged() {
    if (ui->errorLabel->isVisible()) {
        ui->errorLabel->setVisible(false);
    }

    const u64 total_seconds = GetTotalSeconds();
    constexpr u64 max_reasonable_time = 9999ULL * 3600;

    if (total_seconds > max_reasonable_time) {
        ui->errorLabel->setText(tr("Total play time reached maximum."));
        ui->errorLabel->setVisible(true);
    }
}
