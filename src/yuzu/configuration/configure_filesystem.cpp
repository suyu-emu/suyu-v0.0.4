// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "yuzu/configuration/configure_filesystem.h"
#include <filesystem>
#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>
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
    connect(ui->save_directory_button, &QToolButton::pressed, this,
            [this] { SetSaveDirectory(); });
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
    ui->save_directory_edit->setText(
        QString::fromStdString(Common::FS::GetEdenPathString(Common::FS::EdenPath::SaveDir)));
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
    Common::FS::SetEdenPath(Common::FS::EdenPath::SaveDir,
                            ui->save_directory_edit->text().toStdString());
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
    case DirectoryTarget::Save:
        caption = tr("Select Save Data Directory...");
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

void ConfigureFilesystem::SetSaveDirectory() {
    const QString current_path = ui->save_directory_edit->text();
    const QString nand_path = ui->nand_directory_edit->text();

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Save Data Directory"));
    msgBox.setText(tr("Choose an action for the save data directory:"));

    QPushButton* customButton = msgBox.addButton(tr("Set Custom Path"), QMessageBox::ActionRole);
    QPushButton* resetButton = msgBox.addButton(tr("Reset to NAND"), QMessageBox::ActionRole);
    msgBox.addButton(QMessageBox::Cancel);

    msgBox.exec();

    if (msgBox.clickedButton() == customButton) {
        QString new_path = QFileDialog::getExistingDirectory(
            this, tr("Select Save Data Directory..."), current_path);

        if (new_path.isNull() || new_path.isEmpty()) {
            return;
        }

        if (new_path.back() != QChar::fromLatin1('/')) {
            new_path.append(QChar::fromLatin1('/'));
        }

        if (new_path != current_path) {
            PromptSaveMigration(current_path, new_path);
            ui->save_directory_edit->setText(new_path);
        }
    } else if (msgBox.clickedButton() == resetButton) {
        if (current_path != nand_path) {
            PromptSaveMigration(current_path, nand_path);
            ui->save_directory_edit->setText(nand_path);
        }
    }
}

void ConfigureFilesystem::PromptSaveMigration(const QString& from_path, const QString& to_path) {
    namespace fs = std::filesystem;

    const fs::path source_save_dir = fs::path(from_path.toStdString()) / "user" / "save";
    const fs::path dest_save_dir = fs::path(to_path.toStdString()) / "user" / "save";

    std::error_code ec;

    bool source_has_saves = false;
    if (Common::FS::Exists(source_save_dir)) {
        bool source_empty = fs::is_empty(source_save_dir, ec);
        source_has_saves = !ec && !source_empty;
    }

    // Check if destination already has saves
    bool dest_has_saves = false;
    if (Common::FS::Exists(dest_save_dir)) {
        bool dest_empty = fs::is_empty(dest_save_dir, ec);
        dest_has_saves = !ec && !dest_empty;
    }

    if (!source_has_saves) {
        return;
    }

    QString message;
    if (dest_has_saves) {
        message = tr("Save data exists in both the old and new locations.\n\n"
                     "Old: %1\n"
                     "New: %2\n\n"
                     "Would you like to migrate saves from the old location?\n"
                     "WARNING: This will overwrite any conflicting saves in the new location!")
                      .arg(QString::fromStdString(source_save_dir.string()))
                      .arg(QString::fromStdString(dest_save_dir.string()));
    } else {
        message = tr("Would you like to migrate your save data to the new location?\n\n"
                     "From: %1\n"
                     "To: %2")
                      .arg(QString::fromStdString(source_save_dir.string()))
                      .arg(QString::fromStdString(dest_save_dir.string()));
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Migrate Save Data"), message,
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

    if (reply != QMessageBox::Yes) {
        return;
    }

    QProgressDialog progress(tr("Migrating save data..."), tr("Cancel"), 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.show();

    if (!Common::FS::Exists(dest_save_dir)) {
        if (!Common::FS::CreateDirs(dest_save_dir)) {
            progress.close();
            QMessageBox::warning(this, tr("Migration Failed"),
                                 tr("Failed to create destination directory."));
            return;
        }
    }

    fs::copy(source_save_dir, dest_save_dir,
             fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);

    progress.close();

    if (ec) {
        QMessageBox::warning(this, tr("Migration Failed"),
                             tr("Failed to migrate save data:\n%1")
                                 .arg(QString::fromStdString(ec.message())));
        return;
    }

    QMessageBox::StandardButton deleteReply = QMessageBox::question(
        this, tr("Migration Complete"),
        tr("Save data has been migrated successfully.\n\n"
           "Would you like to delete the old save data?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (deleteReply == QMessageBox::Yes) {
        Common::FS::RemoveDirRecursively(source_save_dir);
    }
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
