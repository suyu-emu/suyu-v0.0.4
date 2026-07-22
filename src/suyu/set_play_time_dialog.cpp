// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "set_play_time_dialog.h"
#include "ui_set_play_time_dialog.h"

SetPlayTimeDialog::SetPlayTimeDialog(QWidget* parent, u64 current_play_time)
    : QDialog(parent), ui{std::make_unique<Ui::SetPlayTimeDialog>()} {
    ui->setupUi(this);

    const u64 hours = current_play_time / 3600;
    const u64 minutes = (current_play_time % 3600) / 60;
    const u64 seconds = current_play_time % 60;

    ui->hoursSpinBox->setValue(static_cast<int>(hours));
    ui->minutesSpinBox->setValue(static_cast<int>(minutes));
    ui->secondsSpinBox->setValue(static_cast<int>(seconds));

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
