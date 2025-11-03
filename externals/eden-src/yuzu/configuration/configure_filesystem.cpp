// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "yuzu/configuration/configure_filesystem.h"
#include <QFileDialog>
#include <QMessageBox>
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/settings.h"
#include "qt_common/qt_compat.h"
#include "qt_common/util/game.h"
#include "qt_common/config/uisettings.h"
#include "ui_configure_filesystem.h"

ConfigureFilesystem::ConfigureFilesystem(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureFilesystem>()) {
    ui->setupUi(this);
    SetConfiguration();

    connect(ui->nand_directory_button, &QToolButton::pressed, this,
            [this] { SetDirectory(DirectoryTarget::NAND, ui->nand_directory_edit); });
    connect(ui->sdmc_directory_button, &QToolButton::pressed, this,
            [this] { SetDirectory(DirectoryTarget::SD, ui->sdmc_directory_edit); });
    connect(ui->gamecard_path_button, &QToolButton::pressed, this,
            [this] { SetDirectory(DirectoryTarget::Gamecard, ui->gamecard_path_edit); });
    connect(ui->dump_path_button, &QToolButton::pressed, this,
            [this] { SetDirectory(DirectoryTarget::Dump, ui->dump_path_edit); });
    connect(ui->load_path_button, &QToolButton::pressed, this,
            [this] { SetDirectory(DirectoryTarget::Load, ui->load_path_edit); });

    connect(ui->reset_game_list_cache, &QPushButton::pressed, this,
            &ConfigureFilesystem::ResetMetadata);

    connect(ui->gamecard_inserted, &QCheckBox::STATE_CHANGED, this,
            &ConfigureFilesystem::UpdateEnabledControls);
    connect(ui->gamecard_current_game, &QCheckBox::STATE_CHANGED, this,
            &ConfigureFilesystem::UpdateEnabledControls);
}

ConfigureFilesystem::~ConfigureFilesystem() = default;

void ConfigureFilesystem::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigureFilesystem::SetConfiguration() {
    ui->nand_directory_edit->setText(
        QString::fromStdString(Common::FS::GetEdenPathString(Common::FS::EdenPath::NANDDir)));
    ui->sdmc_directory_edit->setText(
        QString::fromStdString(Common::FS::GetEdenPathString(Common::FS::EdenPath::SDMCDir)));
    ui->gamecard_path_edit->setText(
        QString::fromStdString(Settings::values.gamecard_path.GetValue()));
    ui->dump_path_edit->setText(
        QString::fromStdString(Common::FS::GetEdenPathString(Common::FS::EdenPath::DumpDir)));
    ui->load_path_edit->setText(
        QString::fromStdString(Common::FS::GetEdenPathString(Common::FS::EdenPath::LoadDir)));

    ui->gamecard_inserted->setChecked(Settings::values.gamecard_inserted.GetValue());
    ui->gamecard_current_game->setChecked(Settings::values.gamecard_current_game.GetValue());
    ui->dump_exefs->setChecked(Settings::values.dump_exefs.GetValue());
    ui->dump_nso->setChecked(Settings::values.dump_nso.GetValue());

    ui->cache_game_list->setChecked(UISettings::values.cache_game_list.GetValue());

    UpdateEnabledControls();
}

void ConfigureFilesystem::ApplyConfiguration() {
    Common::FS::SetEdenPath(Common::FS::EdenPath::NANDDir,
                            ui->nand_directory_edit->text().toStdString());
    Common::FS::SetEdenPath(Common::FS::EdenPath::SDMCDir,
                            ui->sdmc_directory_edit->text().toStdString());
    Common::FS::SetEdenPath(Common::FS::EdenPath::DumpDir,
                            ui->dump_path_edit->text().toStdString());
    Common::FS::SetEdenPath(Common::FS::EdenPath::LoadDir,
                            ui->load_path_edit->text().toStdString());

    Settings::values.gamecard_inserted = ui->gamecard_inserted->isChecked();
    Settings::values.gamecard_current_game = ui->gamecard_current_game->isChecked();
    Settings::values.dump_exefs = ui->dump_exefs->isChecked();
    Settings::values.dump_nso = ui->dump_nso->isChecked();

    UISettings::values.cache_game_list = ui->cache_game_list->isChecked();
}

void ConfigureFilesystem::SetDirectory(DirectoryTarget target, QLineEdit* edit) {
    QString caption;

    switch (target) {
    case DirectoryTarget::NAND:
        caption = tr("Select Emulated NAND Directory...");
        break;
    case DirectoryTarget::SD:
        caption = tr("Select Emulated SD Directory...");
        break;
    case DirectoryTarget::Gamecard:
        caption = tr("Select Gamecard Path...");
        break;
    case DirectoryTarget::Dump:
        caption = tr("Select Dump Directory...");
        break;
    case DirectoryTarget::Load:
        caption = tr("Select Mod Load Directory...");
        break;
    }

    QString str;
    if (target == DirectoryTarget::Gamecard) {
        str = QFileDialog::getOpenFileName(this, caption, QFileInfo(edit->text()).dir().path(),
                                           QStringLiteral("NX Gamecard;*.xci"));
    } else {
        str = QFileDialog::getExistingDirectory(this, caption, edit->text());
    }

    if (str.isNull() || str.isEmpty()) {
        return;
    }

    if (str.back() != QChar::fromLatin1('/')) {
        str.append(QChar::fromLatin1('/'));
    }

    edit->setText(str);
}

void ConfigureFilesystem::ResetMetadata() {
    QtCommon::Game::ResetMetadata();
}

void ConfigureFilesystem::UpdateEnabledControls() {
    ui->gamecard_current_game->setEnabled(ui->gamecard_inserted->isChecked());
    ui->gamecard_path_edit->setEnabled(ui->gamecard_inserted->isChecked() &&
                                       !ui->gamecard_current_game->isChecked());
    ui->gamecard_path_button->setEnabled(ui->gamecard_inserted->isChecked() &&
                                         !ui->gamecard_current_game->isChecked());
}

void ConfigureFilesystem::RetranslateUI() {
    ui->retranslateUi(this);
}
