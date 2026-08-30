// SPDX-FileCopyrightText: Copyright 2026 suyu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <filesystem>
#include "qt_common/abstract/frontend.h"
#include "qt_common/util/fs.h"
#include "ryujinx_dialog.h"
#include "ui_ryujinx_dialog.h"

RyujinxDialog::RyujinxDialog(std::filesystem::path suyu_path, std::filesystem::path ryu_path,
                             QWidget* parent)
    : QDialog(parent), ui(new Ui::RyujinxDialog), m_suyu(suyu_path.make_preferred()),
      m_ryu(ryu_path.make_preferred()) {
    ui->setupUi(this);

    connect(ui->suyu, &QPushButton::clicked, this, &RyujinxDialog::fromSuyu);
    connect(ui->ryujinx, &QPushButton::clicked, this, &RyujinxDialog::fromRyujinx);
    connect(ui->cancel, &QPushButton::clicked, this, &RyujinxDialog::reject);
}

RyujinxDialog::~RyujinxDialog() {
    delete ui;
}

void RyujinxDialog::fromSuyu() {
    accept();

    // Workaround: Ryujinx deletes and re-creates its directory structure???
    // So we just copy suyu's data to Ryujinx and then link the other way
    namespace fs = std::filesystem;
    try {
        fs::remove_all(m_ryu);
        fs::create_directories(m_ryu);
        fs::copy(m_suyu, m_ryu, fs::copy_options::recursive);
    } catch (std::exception& e) {
        QtCommon::Frontend::Critical(
            tr("Failed to link save data"),
            tr("OS returned error: %1").arg(QString::fromStdString(e.what())));
    }

    // ?ploo
    QtCommon::FS::LinkRyujinx(m_ryu, m_suyu);
}

void RyujinxDialog::fromRyujinx() {
    accept();
    QtCommon::FS::LinkRyujinx(m_ryu, m_suyu);
}
