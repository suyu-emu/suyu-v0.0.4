// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>
#include <QStandardItemModel>

namespace Ui {
class ModSelectDialog;
}

class ModSelectDialog : public QDialog {
    Q_OBJECT

public:
    explicit ModSelectDialog(const QStringList &mods, QWidget* parent = nullptr);
    ~ModSelectDialog();

signals:
    void modsSelected(const QStringList &mods);
private:
    Ui::ModSelectDialog* ui;

    QStandardItemModel* item_model;
};
