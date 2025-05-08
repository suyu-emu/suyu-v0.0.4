// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// SPDX-FileCopyrightText: Copyright 2025 eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QMessageBox>
#include <QPushButton>
#include <QString>
#include <QTranslator>
#include "common/fs/path_util.h"
#include "user_data_migration.h"

// Needs to be included at the end due to https://bugreports.qt.io/browse/QTBUG-73263
#include <QCheckBox>
#include <filesystem>

namespace fs = std::filesystem;

UserDataMigrator::UserDataMigrator(QMainWindow* main_window) {
    // NOTE: Logging is not initialized yet, do not produce logs here.

    // Check migration if config directory does not exist
    // TODO: ProfileManager messes with us a bit here, and force-creates the /nand/system/save/8000000000000010/su/avators/profiles.dat
    // file. Find a way to reorder operations and have it create after this guy runs.
    if (!fs::is_directory(Common::FS::GetEdenPath(Common::FS::EdenPath::ConfigDir))) {
        ShowMigrationPrompt(main_window);
    }
}

void UserDataMigrator::ShowMigrationPrompt(QMainWindow* main_window) {
    namespace fs = std::filesystem;

    const QString migration_prompt_message =
        main_window->tr("Would you like to migrate your data for use in Eden?\n"
                        "Select the corresponding button to migrate data from that emulator.\n"
                        "(This may take a while; The old data will not be deleted)\n\n"
                        "Clearing shader cache is recommended for all users.\nDo not uncheck unless you know what you're doing.");

    bool any_found = false;

    QMessageBox migration_prompt;
    migration_prompt.setWindowTitle(main_window->tr("Migration"));
    migration_prompt.setIcon(QMessageBox::Information);

    QCheckBox *clear_shaders = new QCheckBox(&migration_prompt);
    clear_shaders->setText(main_window->tr("Clear Shader Cache"));
    clear_shaders->setChecked(true);
    migration_prompt.setCheckBox(clear_shaders);

    // Reflection would make this code 10x better
    // but for now... MACRO MADNESS!!!!
    QMap<QString, bool> found;
    QMap<QString, LegacyEmu> legacyMap;
    QMap<QString, QAbstractButton *> buttonMap;

#define EMU_MAP(name) const bool name##_found = fs::is_directory(Common::FS::GetLegacyPath(Common::FS::LegacyPath::name##Dir)); \
    legacyMap[main_window->tr(#name)] = LegacyEmu::name; \
    found[main_window->tr(#name)] = name##_found; \
    if (name##_found) any_found = true;

    EMU_MAP(Citron)
    EMU_MAP(Sudachi)
    EMU_MAP(Yuzu)
    EMU_MAP(Suyu)

#undef EMU_MAP

    if (any_found) {
        QString promptText = main_window->tr("Eden has detected user data for the following emulators:");
        QMapIterator iter(found);

        while (iter.hasNext()) {
            iter.next();
            if (!iter.value()) continue;

            buttonMap[iter.key()] = migration_prompt.addButton(iter.key(), QMessageBox::YesRole);
            promptText.append(main_window->tr("\n- %1").arg(iter.key()));
        }

        promptText.append(main_window->tr("\n\n"));

        migration_prompt.setText(promptText + migration_prompt_message);
        migration_prompt.addButton(QMessageBox::No);

        migration_prompt.exec();

        QMapIterator buttonIter(buttonMap);

        while (buttonIter.hasNext()) {
            buttonIter.next();
            if (buttonIter.value() == migration_prompt.clickedButton()) {
                MigrateUserData(main_window, legacyMap[iter.key()], clear_shaders->isChecked());
                return;
            }
        }

        // If we're here, the user chose not to migrate
        ShowMigrationCancelledMessage(main_window);
    }

    else // no other data was found
        return;
}

void UserDataMigrator::ShowMigrationCancelledMessage(QMainWindow* main_window) {
    QMessageBox::information(
        main_window, main_window->tr("Migration"),
        main_window
            ->tr("You can manually re-trigger this prompt by deleting the "
                 "new config directory:\n"
                 "%1")
            .arg(QString::fromStdString(Common::FS::GetEdenPathString(Common::FS::EdenPath::ConfigDir))),
        QMessageBox::Ok);
}

void UserDataMigrator::MigrateUserData(QMainWindow* main_window,
                                       const LegacyEmu selected_legacy_emu, const bool clear_shader_cache) {
    namespace fs = std::filesystem;
    const auto copy_options = fs::copy_options::update_existing | fs::copy_options::recursive;

    std::string legacy_user_dir;
    std::string legacy_config_dir;
    std::string legacy_cache_dir;

#define LEGACY_EMU(emu) case LegacyEmu::emu: \
    legacy_user_dir = Common::FS::GetLegacyPath(Common::FS::LegacyPath::emu##Dir).string(); \
    legacy_config_dir = Common::FS::GetLegacyPath(Common::FS::LegacyPath::emu##ConfigDir).string(); \
    legacy_cache_dir = Common::FS::GetLegacyPath(Common::FS::LegacyPath::emu##CacheDir).string(); \
    break;

    switch (selected_legacy_emu) {
        LEGACY_EMU(Citron)
        LEGACY_EMU(Sudachi)
        LEGACY_EMU(Yuzu)
        LEGACY_EMU(Suyu)
    }

#undef LEGACY_EMU

    fs::copy(legacy_user_dir, Common::FS::GetEdenPath(Common::FS::EdenPath::EdenDir), copy_options);

    if (fs::is_directory(legacy_config_dir)) {
        fs::copy(legacy_config_dir, Common::FS::GetEdenPath(Common::FS::EdenPath::ConfigDir),
                 copy_options);
    }
    if (fs::is_directory(legacy_cache_dir)) {
        fs::copy(legacy_cache_dir, Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir),
                 copy_options);
    }

    // Delete and re-create shader dir
    if (clear_shader_cache) {
        fs::remove_all(Common::FS::GetEdenPath(Common::FS::EdenPath::ShaderDir));
        fs::create_directory(Common::FS::GetEdenPath(Common::FS::EdenPath::ShaderDir));
    }

    QMessageBox::information(
        main_window, main_window->tr("Migration"),
        main_window
            ->tr("Data was migrated successfully.\n\n"
                 "If you wish to clean up the files which were left in the old "
                 "data location, you can do so by deleting the following directory:\n"
                 "%1")
            .arg(QString::fromStdString(legacy_user_dir)),
        QMessageBox::Ok);
}
