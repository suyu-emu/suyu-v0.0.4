// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "user_data_migration.h"
#include <QMessageBox>
#include <QPushButton>
#include <QString>
#include <QTranslator>
#include "common/fs/path_util.h"
#include "qt_common/qt_string_lookup.h"
#include "yuzu/migration_dialog.h"

// Needs to be included at the end due to https://bugreports.qt.io/browse/QTBUG-73263
#include <QButtonGroup>
#include <QCheckBox>
#include <QGuiApplication>
#include <QProgressDialog>
#include <QRadioButton>
#include <QThread>
#include <filesystem>

UserDataMigrator::UserDataMigrator(QMainWindow *main_window)
{
    // NOTE: Logging is not initialized yet, do not produce logs here.

    // Check migration if config directory does not exist
    // TODO: ProfileManager messes with us a bit here, and force-creates the /nand/system/save/8000000000000010/su/avators/profiles.dat
    // file. Find a way to reorder operations and have it create after this guy runs.
    if (!std::filesystem::is_directory(Common::FS::GetEdenPath(Common::FS::EdenPath::ConfigDir))) {
        ShowMigrationPrompt(main_window);
    }
}

void UserDataMigrator::ShowMigrationPrompt(QMainWindow *main_window)
{
    namespace fs = std::filesystem;
    using namespace QtCommon::StringLookup;

    MigrationDialog migration_prompt;
    migration_prompt.setWindowTitle(QObject::tr("Migration"));

    // mutually exclusive
    QButtonGroup *group = new QButtonGroup(&migration_prompt);

    // MACRO MADNESS

#define BUTTON(clazz, name, text, tooltip, checkState) \
    clazz *name = new clazz(&migration_prompt); \
    name->setText(text); \
    name->setToolTip(Lookup(tooltip)); \
    name->setChecked(checkState); \
    migration_prompt.addBox(name);

    BUTTON(QCheckBox, clear_shaders, QObject::tr("Clear Shader Cache"), MigrationTooltipClearShader, true)

    u32 id = 0;

#define RADIO(name, text, tooltip, checkState) \
    BUTTON(QRadioButton, name, text, tooltip, checkState) \
    group->addButton(name, ++id);

    RADIO(keep_old,  QObject::tr("Keep Old Data"), MigrationTooltipKeepOld, true)
    RADIO(clear_old, QObject::tr("Clear Old Data"), MigrationTooltipClearOld, false)
    RADIO(link_old,  QObject::tr("Link Old Directory"), MigrationTooltipLinkOld, false)

#undef RADIO
#undef BUTTON

    std::vector<Emulator> found{};

    for (const Emulator &emu : legacy_emus)
        if (fs::is_directory(emu.get_user_dir()))
            found.emplace_back(emu);

    if (found.empty()) {
        return;
    }

    // makes my life easier
    qRegisterMetaType<Emulator>();

    QString prompt_text = Lookup(MigrationPromptPrefix);

    // natural language processing is a nightmare
    for (const Emulator &emu : found) {
        prompt_text = prompt_text % QStringLiteral("\n ") % emu.name();

        QAbstractButton *button = migration_prompt.addButton(emu.name());

        // This is cursed, but it's actually the most efficient way by a mile
        button->setProperty("emulator", QVariant::fromValue(emu));
    }

    prompt_text.append(QObject::tr("\n\n"));
    prompt_text = prompt_text % QStringLiteral("\n\n") % Lookup(MigrationPrompt);

    migration_prompt.setText(prompt_text);
    migration_prompt.addButton(QObject::tr("No"), true);

    migration_prompt.exec();

    QAbstractButton *button = migration_prompt.clickedButton();

    if (button->text() == QObject::tr("No")) {
        return ShowMigrationCancelledMessage(main_window);
    }

    MigrationWorker::MigrationStrategy strategy = static_cast<MigrationWorker::MigrationStrategy>(
        group->checkedId());

    selected_emu = button->property("emulator").value<Emulator>();

    MigrateUserData(main_window,
                    clear_shaders->isChecked(),
                    strategy);
}

void UserDataMigrator::ShowMigrationCancelledMessage(QMainWindow *main_window)
{
    QMessageBox::information(main_window,
                             QObject::tr("Migration"),
                             QObject::tr("You can manually re-trigger this prompt by deleting the "
                                         "new config directory:\n%1")
                                 .arg(QString::fromStdString(Common::FS::GetEdenPathString(
                                     Common::FS::EdenPath::ConfigDir))),
                             QMessageBox::Ok);
}

void UserDataMigrator::MigrateUserData(QMainWindow *main_window,
                                       const bool clear_shader_cache,
                                       const MigrationWorker::MigrationStrategy strategy)
{
    // Create a dialog to let the user know it's migrating
    QProgressDialog *progress = new QProgressDialog(main_window);
    progress->setWindowTitle(QObject::tr("Migrating"));
    progress->setLabelText(QObject::tr("Migrating, this may take a while..."));
    progress->setRange(0, 0);
    progress->setCancelButton(nullptr);
    progress->setWindowModality(Qt::WindowModality::ApplicationModal);

    QThread *thread = new QThread(main_window);
    MigrationWorker *worker = new MigrationWorker(selected_emu, clear_shader_cache, strategy);
    worker->moveToThread(thread);

    thread->connect(thread, &QThread::started, worker, &MigrationWorker::process);

    thread->connect(worker,
                    &MigrationWorker::finished,
                    progress,
                    [=, this](const QString &success_text, const std::string &path) {
                        progress->close();
                        QMessageBox::information(main_window,
                                                 QObject::tr("Migration"),
                                                 success_text,
                                                 QMessageBox::Ok);

                        migrated = true;
                        thread->quit();
                    });

    thread->connect(worker, &MigrationWorker::finished, worker, &QObject::deleteLater);
    thread->connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    thread->start();
    progress->exec();
}
