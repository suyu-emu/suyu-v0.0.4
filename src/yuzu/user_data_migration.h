// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QMainWindow>
#include "../yuzu/migration_worker.h"

// TODO(crueter): Quick implementation
class UserDataMigrator {
public:
    UserDataMigrator(QMainWindow* main_window);

    bool migrated{false};
    Emulator selected_emu;

private:
    void ShowMigrationPrompt(QMainWindow* main_window);
    void ShowMigrationCancelledMessage(QMainWindow* main_window);
    void MigrateUserData(QMainWindow* main_window,
                         const bool clear_shader_cache,
                         const MigrationWorker::MigrationStrategy strategy);
};
