// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ryujinx_dialog.h"
#include "qt_common/util/fs.h"
#include "ui_ryujinx_dialog.h"
#include <filesystem>

namespace fs = std::filesystem;

RyujinxDialog::RyujinxDialog(std::filesystem::path eden_path,
                             std::filesystem::path ryu_path,
                             QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RyujinxDialog)
    , m_eden(eden_path.make_preferred())
    , m_ryu(ryu_path.make_preferred())
{
    ui->setupUi(this);

    connect(ui->eden, &QPushButton::clicked, this, &RyujinxDialog::fromEden);
    connect(ui->ryujinx, &QPushButton::clicked, this, &RyujinxDialog::fromRyujinx);
}

RyujinxDialog::~RyujinxDialog()
{
    delete ui;
}

void RyujinxDialog::fromEden()
{
    accept();
    QtCommon::FS::LinkRyujinx(m_eden, m_ryu);
}

void RyujinxDialog::fromRyujinx()
{
    accept();
    QtCommon::FS::LinkRyujinx(m_ryu, m_eden);
}
