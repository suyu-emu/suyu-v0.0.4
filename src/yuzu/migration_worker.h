// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MIGRATION_WORKER_H
#define MIGRATION_WORKER_H

#include <QObject>
#include "common/fs/path_util.h"

typedef struct Emulator {
    const char *m_name;

    Common::FS::EmuPath e_user_dir;
    Common::FS::EmuPath e_config_dir;
    Common::FS::EmuPath e_cache_dir;

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

#define STRUCT_EMU(name, enumName) Emulator{name, Common::FS::enumName##Dir, Common::FS::enumName##ConfigDir, Common::FS::enumName##CacheDir}

static constexpr std::array<Emulator, 4> legacy_emus = {
    STRUCT_EMU(QT_TR_NOOP("Citron"), Citron),
    STRUCT_EMU(QT_TR_NOOP("Sudachi"), Sudachi),
    STRUCT_EMU(QT_TR_NOOP("Suyu"), Suyu),
    STRUCT_EMU(QT_TR_NOOP("Yuzu"), Yuzu),
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

    MigrationWorker(const Emulator selected_emu,
                    const bool clear_shader_cache,
                    const MigrationStrategy strategy);

public slots:
    void process();

signals:
    void finished(const QString &success_text, const std::string &user_dir);
    void error(const QString &error_message);

private:
    Emulator selected_emu;
    bool clear_shader_cache;
    MigrationStrategy strategy;
    QString success_text = tr("Data was migrated successfully.");
};

#endif // MIGRATION_WORKER_H
