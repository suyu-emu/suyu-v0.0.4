// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
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
    lbl_instructions_->setText(QStringLiteral(
        "<p>This build is configured for external decryption or pre-decrypted games.</p>"
        "<p>Configure your external decryption tool from the Tools menu, then refresh status.</p>"
        "<p><i>SuyuEclipse does not provide keys or enable piracy.</i></p>"));
    layout->addWidget(lbl_instructions_);

    progress_ = new QProgressBar(this);
    progress_->setRange(0, 0); // indeterminate
    progress_->setVisible(false);
    layout->addWidget(progress_);

    auto* btn_layout = new QHBoxLayout();
    btn_browse_ = new QPushButton(QStringLiteral("External Tool Info..."), this);
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
    keys_valid_ = true;
    lbl_status_->setText(
        QStringLiteral("<span style='color:green;font-size:14pt;'>External/pre-decrypted mode active</span>"));

    emit KeysStatusChanged(keys_valid_);
}

void DecryptionFlowWidget::OnBrowseKeys() {
    QMessageBox::information(
        this, QStringLiteral("External Decryption"),
        QStringLiteral("Use Tools > Configure External Decryption Tool, or launch pre-decrypted games directly."));
}

void DecryptionFlowWidget::OnRefreshStatus() {
    DetectKeys();
}
