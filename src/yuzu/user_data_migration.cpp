// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// SPDX-FileCopyrightText: Copyright 2025 eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "user_data_migration.h"
#include <QMessageBox>
#include <QPushButton>
#include <QString>
#include <QTranslator>
#include "common/fs/path_util.h"
#include "migration_dialog.h"

// Needs to be included at the end due to https://bugreports.qt.io/browse/QTBUG-73263
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QProgressDialog>
#include <QRadioButton>
#include <QThread>
#include <filesystem>

namespace fs = std::filesystem;

UserDataMigrator::UserDataMigrator(QMainWindow *main_window)
{
    // NOTE: Logging is not initialized yet, do not produce logs here.

    // Check migration if config directory does not exist
    // TODO: ProfileManager messes with us a bit here, and force-creates the /nand/system/save/8000000000000010/su/avators/profiles.dat
    // file. Find a way to reorder operations and have it create after this guy runs.
    if (!fs::is_directory(Common::FS::GetEdenPath(Common::FS::EdenPath::ConfigDir))) {
        ShowMigrationPrompt(main_window);
    }
}

void UserDataMigrator::ShowMigrationPrompt(QMainWindow *main_window)
{
    namespace fs = std::filesystem;

    const QString migration_prompt_message = main_window->tr(
        "Would you like to migrate your data for use in Eden?\n"
        "Select the corresponding button to migrate data from that emulator.\n"
        "This may take a while.");

    bool any_found = false;

    MigrationDialog migration_prompt;
    migration_prompt.setWindowTitle(main_window->tr("Migration"));

    QCheckBox *clear_shaders = new QCheckBox(&migration_prompt);
    clear_shaders->setText(main_window->tr("Clear Shader Cache"));
    clear_shaders->setToolTip(main_window->tr(
        "Clearing shader cache is recommended for all users.\nDo not uncheck unless you know what "
        "you're doing."));
    clear_shaders->setChecked(true);

    QRadioButton *keep_old = new QRadioButton(&migration_prompt);
    keep_old->setText(main_window->tr("Keep Old Data"));
    keep_old->setToolTip(
        main_window->tr("Keeps the old data directory. This is recommended if you aren't\n"
                        "space-constrained and want to keep separate data for the old emulator."));
    keep_old->setChecked(true);

    QRadioButton *clear_old = new QRadioButton(&migration_prompt);
    clear_old->setText(main_window->tr("Clear Old Data"));
    clear_old->setToolTip(main_window->tr("Deletes the old data directory.\nThis is recommended on "
                                          "devices with space constraints."));
    clear_old->setChecked(false);

    QRadioButton *link = new QRadioButton(&migration_prompt);
    link->setText(main_window->tr("Link Old Directory"));
    link->setToolTip(
        main_window->tr("Creates a filesystem link between the old directory and Eden directory.\n"
                        "This is recommended if you want to share data between emulators.."));
    link->setChecked(false);

    // Link and Clear Old are mutually exclusive
    QButtonGroup *group = new QButtonGroup(&migration_prompt);
    group->addButton(keep_old);
    group->addButton(clear_old);
    group->addButton(link);

    migration_prompt.addBox(clear_shaders);
    migration_prompt.addBox(keep_old);
    migration_prompt.addBox(clear_old);
    migration_prompt.addBox(link);

    // Reflection would make this code 10x better
    // but for now... MACRO MADNESS!!!!
    QMap<QString, bool> found;
    QMap<QString, MigrationWorker::LegacyEmu> legacyMap;
    QMap<QString, QAbstractButton *> buttonMap;

#define EMU_MAP(name) \
    const bool name##_found = fs::is_directory( \
        Common::FS::GetLegacyPath(Common::FS::LegacyPath::name##Dir)); \
    legacyMap[main_window->tr(#name)] = MigrationWorker::LegacyEmu::name; \
    found[main_window->tr(#name)] = name##_found; \
    if (name##_found) \
        any_found = true;

    EMU_MAP(Citron)
    EMU_MAP(Sudachi)
    EMU_MAP(Yuzu)
    EMU_MAP(Suyu)

#undef EMU_MAP

    if (any_found) {
        QString promptText = main_window->tr(
            "Eden has detected user data for the following emulators:");
        QMapIterator iter(found);

        while (iter.hasNext()) {
            iter.next();
            if (!iter.value())
                continue;

            QAbstractButton *button = migration_prompt.addButton(iter.key());
            // TMP: disable citron
            if (iter.key() == main_window->tr("Citron")) {
                button->setEnabled(false);
                button->setToolTip(
                    main_window->tr("Citron migration is known to cause issues. It's recommended "
                                    "to manually set up your data again."));
            }
            buttonMap[iter.key()] = button;
            promptText.append(main_window->tr("\n- %1").arg(iter.key()));
        }

        promptText.append(main_window->tr("\n\n"));

        migration_prompt.setText(promptText + migration_prompt_message);
        migration_prompt.addButton(main_window->tr("No"), true);

        migration_prompt.exec();

        MigrationWorker::MigrationStrategy strategy;
        if (link->isChecked()) {
            strategy = MigrationWorker::MigrationStrategy::Link;
        } else if (clear_old->isChecked()) {
            strategy = MigrationWorker::MigrationStrategy::Move;
        } else {
            strategy = MigrationWorker::MigrationStrategy::Copy;
        }

        QMapIterator buttonIter(buttonMap);

        while (buttonIter.hasNext()) {
            buttonIter.next();
            if (buttonIter.value() == migration_prompt.clickedButton()) {
                MigrateUserData(main_window,
                                legacyMap[buttonIter.key()],
                                clear_shaders->isChecked(),
                                strategy);
                return;
            }
        }

        // If we're here, the user chose not to migrate
        ShowMigrationCancelledMessage(main_window);
    }

    else // no other data was found
        return;
}

void UserDataMigrator::ShowMigrationCancelledMessage(QMainWindow *main_window)
{
    QMessageBox::information(main_window,
                             main_window->tr("Migration"),
                             main_window
                                 ->tr("You can manually re-trigger this prompt by deleting the "
                                      "new config directory:\n"
                                      "%1")
                                 .arg(QString::fromStdString(Common::FS::GetEdenPathString(
                                     Common::FS::EdenPath::ConfigDir))),
                             QMessageBox::Ok);
}

void UserDataMigrator::MigrateUserData(QMainWindow *main_window,
                                       const MigrationWorker::LegacyEmu selected_legacy_emu,
                                       const bool clear_shader_cache,
                                       const MigrationWorker::MigrationStrategy strategy)
{
    // Create a dialog to let the user know it's migrating, some users noted confusion.
    QProgressDialog *progress = new QProgressDialog(main_window);
    progress->setWindowTitle(main_window->tr("Migrating"));
    progress->setLabelText(main_window->tr("Migrating, this may take a while..."));
    progress->setRange(0, 0);
    progress->setCancelButton(nullptr);
    progress->setWindowModality(Qt::WindowModality::ApplicationModal);

    QThread *thread = new QThread(main_window);
    MigrationWorker *worker = new MigrationWorker(selected_legacy_emu, clear_shader_cache, strategy);
    worker->moveToThread(thread);

    thread->connect(thread, &QThread::started, worker, &MigrationWorker::process);

    thread->connect(worker, &MigrationWorker::finished, progress, [=](const QString &success_text) {
        progress->close();
        QMessageBox::information(main_window,
                                 main_window->tr("Migration"),
                                 success_text,
                                 QMessageBox::Ok);

        thread->quit();
    });

    thread->connect(worker, &MigrationWorker::finished, worker, &QObject::deleteLater);
    thread->connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    thread->start();
    progress->exec();
}
