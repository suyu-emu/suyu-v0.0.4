// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MIGRATION_WORKER_H
#define MIGRATION_WORKER_H

#include <QObject>
#include "common/fs/path_util.h"

using namespace Common::FS;

typedef struct Emulator {
    const char *m_name;

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

    const QString name() const { return QObject::tr(m_name);
    }

    const QString lower_name() const { return name().toLower();
    }
} Emulator;

static constexpr std::array<Emulator, 4> legacy_emus = {
    Emulator{QT_TR_NOOP("Citron"), CitronDir, CitronConfigDir, CitronCacheDir},
    Emulator{QT_TR_NOOP("Sudachi"), SudachiDir, SudachiConfigDir, SudachiCacheDir},
    Emulator{QT_TR_NOOP("Suyu"), SuyuDir, SuyuConfigDir, SuyuCacheDir},
    Emulator{QT_TR_NOOP("Yuzu"), YuzuDir, YuzuConfigDir, YuzuCacheDir},
};

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
