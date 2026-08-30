// SPDX-FileCopyrightText: Copyright 2025 suyu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MIGRATION_WORKER_H
#define MIGRATION_WORKER_H

#include <array>
#include <filesystem>
#include <string>

#include <QObject>
#include "common/fs/path_util.h"

struct Emulator {
    const char* m_name{};
    std::filesystem::path user_dir{};
    std::filesystem::path config_dir{};
    std::filesystem::path cache_dir{};

    [[nodiscard]] std::string get_user_dir() const {
        return user_dir.string();
    }

    [[nodiscard]] std::string get_config_dir() const {
        return config_dir.string();
    }

    [[nodiscard]] std::string get_cache_dir() const {
        return cache_dir.string();
    }

    [[nodiscard]] QString name() const {
        return QObject::tr(m_name);
    }

    [[nodiscard]] QString lower_name() const {
        return name().toLower();
    }
};

Q_DECLARE_METATYPE(Emulator)

std::array<Emulator, 4> BuildLegacyEmulators();
extern const std::array<Emulator, 4> legacy_emus;

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
