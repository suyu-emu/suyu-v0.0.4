// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MIGRATION_WORKER_H
#define MIGRATION_WORKER_H

#include <QObject>
#include "common/fs/path_util.h"

using namespace Common::FS;

typedef struct Emulator {
    const char *name;

    LegacyPath e_user_dir;
    LegacyPath e_config_dir;
    LegacyPath e_cache_dir;

    const std::string get_user_dir() const {
        return Common::FS::GetLegacyPath(e_user_dir).string();
    }

    const std::string get_config_dir() const {
        return Common::FS::GetLegacyPath(e_config_dir).string();
    }

    const std::string get_cache_dir() const {
        return Common::FS::GetLegacyPath(e_cache_dir).string();
    }

    const std::string lower_name() const {
        std::string lower_name{name};
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                       [](unsigned char c){ return std::tolower(c); });

        return lower_name;
    }
} Emulator;

#define EMU(name) Emulator{#name, name##Dir, name##ConfigDir, name##CacheDir}
static constexpr std::array<Emulator, 4> legacy_emus = {
    EMU(Citron),
    EMU(Sudachi),
    EMU(Suyu),
    EMU(Yuzu),
};
#undef EMU

class MigrationWorker : public QObject
{
    Q_OBJECT
public:
    enum class MigrationStrategy {
        Copy,
        Move,
        Link,
    };

    MigrationWorker(const Emulator selected_legacy_emu,
                    const bool clear_shader_cache,
                    const MigrationStrategy strategy);

public slots:
    void process();

signals:
    void finished(const QString &success_text, const std::string &user_dir);
    void error(const QString &error_message);

private:
    Emulator selected_legacy_emu;
    bool clear_shader_cache;
    MigrationStrategy strategy;
    QString success_text = tr("Data was migrated successfully.");
};

#endif // MIGRATION_WORKER_H
