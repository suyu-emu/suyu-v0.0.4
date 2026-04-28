// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>
#include "common/net/net.h"

class QRadioButton;
namespace Ui {
class UpdateDialog;
}

class UpdateDialog : public QDialog {
    Q_OBJECT

public:
    explicit UpdateDialog(const Common::Net::Release &release, QWidget* parent = nullptr);
    ~UpdateDialog();

private slots:
    void Download();

private:
    Ui::UpdateDialog* ui;
    QList<QRadioButton *> m_buttons;
    Common::Net::Asset m_asset;
};
