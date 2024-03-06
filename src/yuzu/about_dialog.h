// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project & 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Modified by JuanCStar on 2024/03/06

#pragma once

#include <memory>
#include <QDialog>

namespace Ui {
class AboutDialog;
}

class AboutDialog : public QDialog {
    Q_OBJECT

public:
    explicit AboutDialog(QWidget* parent);
    ~AboutDialog() override;

private:
    std::unique_ptr<Ui::AboutDialog> ui;
};
