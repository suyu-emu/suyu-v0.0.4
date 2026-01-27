// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef RYUJINX_DIALOG_H
#define RYUJINX_DIALOG_H

#include <QDialog>
#include <filesystem>

namespace Ui {
class RyujinxDialog;
}

class RyujinxDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RyujinxDialog(std::filesystem::path eden_path, std::filesystem::path ryu_path, QWidget *parent = nullptr);
    ~RyujinxDialog();

private slots:
    void fromEden();
    void fromRyujinx();

private:
    Ui::RyujinxDialog *ui;
    std::filesystem::path m_eden;
    std::filesystem::path m_ryu;
};

#endif // RYUJINX_DIALOG_H
