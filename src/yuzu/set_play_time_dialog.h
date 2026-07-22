// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <QDialog>
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
