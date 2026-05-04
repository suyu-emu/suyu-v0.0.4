// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>

#include "suyu/decryption_flow.h"

DecryptionFlowWidget::DecryptionFlowWidget(QWidget* parent) : QWidget(parent) {
    SetupUi();
    DetectKeys();
}

DecryptionFlowWidget::~DecryptionFlowWidget() = default;

void DecryptionFlowWidget::SetupUi() {
    auto* layout = new QVBoxLayout(this);

    lbl_status_ = new QLabel(this);
    lbl_status_->setAlignment(Qt::AlignCenter);
    layout->addWidget(lbl_status_);

    lbl_instructions_ = new QLabel(this);
    lbl_instructions_->setWordWrap(true);
    layout->addWidget(lbl_instructions_);

    progress_ = new QProgressBar(this);
    progress_->setRange(0, 0); // indeterminate
    progress_->setVisible(false);
    layout->addWidget(progress_);

    auto* btn_layout = new QHBoxLayout();
    btn_browse_ = new QPushButton(QStringLiteral("Open Keys Folder"), this);
    btn_refresh_ = new QPushButton(QStringLiteral("Refresh"), this);
    btn_layout->addWidget(btn_browse_);
    btn_layout->addWidget(btn_refresh_);
    layout->addLayout(btn_layout);

    connect(btn_browse_, &QPushButton::clicked, this, &DecryptionFlowWidget::OnBrowseKeys);
    connect(btn_refresh_, &QPushButton::clicked, this, &DecryptionFlowWidget::OnRefreshStatus);

    setLayout(layout);
}

bool DecryptionFlowWidget::HasValidKeys() const {
    return keys_valid_;
}

void DecryptionFlowWidget::Refresh() {
    DetectKeys();
}

void DecryptionFlowWidget::DetectKeys() {
    const QString data_dir =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QDir keys_dir(data_dir + QStringLiteral("/suyu/keys/"));

    const QFileInfo prod(keys_dir.filePath(QStringLiteral("prod.keys")));
    const QFileInfo title(keys_dir.filePath(QStringLiteral("title.keys")));

    keys_valid_ = prod.exists() && prod.size() > 0;
    if (keys_valid_) {
        lbl_status_->setText(
            QStringLiteral("<span style='color:green;font-size:14pt;'>Decryption keys detected</span>"));
        lbl_instructions_->setText(
            QStringLiteral("<p><b>prod.keys</b> is installed in <b>%1</b>.</p>"
                           "<p>%2</p>"
                           "<p><i>SuyuEclipse does not provide keys. Dump keys from hardware you own.</i></p>")
                .arg(QDir::toNativeSeparators(keys_dir.absolutePath()),
                     title.exists()
                         ? QStringLiteral("<b>title.keys</b> is also present.")
                         : QStringLiteral("<b>title.keys</b> is optional and was not detected.")));
    } else {
        lbl_status_->setText(
            QStringLiteral("<span style='color:#c0392b;font-size:14pt;'>Decryption keys not found</span>"));
        lbl_instructions_->setText(
            QStringLiteral("<p>Install <b>prod.keys</b> into <b>%1</b> using <b>Tools -> Install Decryption Keys</b>.</p>"
                           "<p>You can still configure an external decryption tool if that workflow suits you better.</p>"
                           "<p><i>SuyuEclipse does not provide keys. Dump keys from hardware you own.</i></p>")
                .arg(QDir::toNativeSeparators(keys_dir.absolutePath())));
    }

    emit KeysStatusChanged(keys_valid_);
}

void DecryptionFlowWidget::OnBrowseKeys() {
    const QString data_dir =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QDir keys_dir(data_dir + QStringLiteral("/suyu/keys/"));
    keys_dir.mkpath(QStringLiteral("."));
    QDesktopServices::openUrl(QUrl::fromLocalFile(keys_dir.absolutePath()));
}

void DecryptionFlowWidget::OnRefreshStatus() {
    DetectKeys();
}
