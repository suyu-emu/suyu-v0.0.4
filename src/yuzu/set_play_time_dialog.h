// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>
#include <memory>
#include "common/common_types.h"

namespace Ui {
class SetPlayTimeDialog;
}

class SetPlayTimeDialog : public QDialog {
    Q_OBJECT

public:
    explicit SetPlayTimeDialog(QWidget* parent, u64 current_play_time);
    ~SetPlayTimeDialog() override;

    u64 GetTotalSeconds() const;

private:
    void OnValueChanged();

    std::unique_ptr<Ui::SetPlayTimeDialog> ui;
};
