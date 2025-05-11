// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// SPDX-FileCopyrightText: Copyright 2025 eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMainWindow>
#include "migration_worker.h"

class UserDataMigrator {
public:
    UserDataMigrator(QMainWindow* main_window);

private:
    void ShowMigrationPrompt(QMainWindow* main_window);
    void ShowMigrationCancelledMessage(QMainWindow* main_window);
    void MigrateUserData(QMainWindow* main_window,
                         const MigrationWorker::LegacyEmu selected_legacy_emu,
                         const bool clear_shader_cache,
                         const MigrationWorker::MigrationStrategy strategy);
};
