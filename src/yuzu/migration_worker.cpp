// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "migration_worker.h"
#include "common/fs/symlink.h"

#include <QMap>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <filesystem>

#include "common/fs/path_util.h"

MigrationWorker::MigrationWorker(const Emulator selected_emu_,
                                 const bool clear_shader_cache_,
                                 const MigrationStrategy strategy_)
    : QObject()
    , selected_emu(selected_emu_)
    , clear_shader_cache(clear_shader_cache_)
    , strategy(strategy_)
{}

void MigrationWorker::process()
{
    namespace fs = std::filesystem;
    constexpr auto copy_options = fs::copy_options::update_existing | fs::copy_options::recursive;

    const fs::path legacy_user_dir = selected_emu.get_user_dir();
    const fs::path legacy_config_dir = selected_emu.get_config_dir();
    const fs::path legacy_cache_dir = selected_emu.get_cache_dir();

    // TODO(crueter): Make these constexpr since they're defaulted
    fs::path eden_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::EdenDir);
    fs::path config_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::ConfigDir);
    fs::path cache_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir);
    fs::path shader_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::ShaderDir);

    eden_dir.make_preferred();
    config_dir.make_preferred();
    cache_dir.make_preferred();
    shader_dir.make_preferred();

    try {
        fs::remove_all(eden_dir);
    } catch (fs::filesystem_error &_) {
        // ignore because linux does stupid crap sometimes
    }

    switch (strategy) {
    case MigrationStrategy::Link:
        // Create symlinks/directory junctions if requested

        // Windows 11 has random permission nonsense to deal with.
        try {
            Common::FS::CreateSymlink(legacy_user_dir, eden_dir);
        } catch (const fs::filesystem_error &e) {
            emit error(tr("Linking the old directory failed. You may need to re-run with "
                          "administrative privileges on Windows.\nOS gave error: %1")
                           .arg(tr(e.what())));
            std::exit(-1);
        }

        // Windows doesn't need any more links, because cache and config
        // are already children of the root directory
#ifndef WIN32
        if (fs::is_directory(legacy_config_dir)) {
            Common::FS::CreateSymlink(legacy_config_dir, config_dir);
        }

        if (fs::is_directory(legacy_cache_dir)) {
            Common::FS::CreateSymlink(legacy_cache_dir, cache_dir);
        }
#endif

        success_text.append(tr("\n\nNote that your configuration and data will be shared with %1.\n"
                               "If this is not desirable, delete the following files:\n%2\n%3\n%4")
                                .arg(selected_emu.name(),
                                     QString::fromStdString(eden_dir.string()),
                                     QString::fromStdString(config_dir.string()),
                                     QString::fromStdString(cache_dir.string())));

        break;
    case MigrationStrategy::Move:
        // Rename directories if deletion is requested (achieves the same result)
        fs::rename(legacy_user_dir, eden_dir);

        // Windows doesn't need any more renames, because cache and config
        // are already children of the root directory
#ifndef WIN32
        if (fs::is_directory(legacy_config_dir)) {
            fs::rename(legacy_config_dir, config_dir);
        }

        if (fs::is_directory(legacy_cache_dir)) {
            fs::rename(legacy_cache_dir, cache_dir);
        }
#endif
        break;
    case MigrationStrategy::Copy:
    default:
        // Default behavior: copy
        fs::copy(legacy_user_dir, eden_dir, copy_options);

        // Windows doesn't need any more copies, because cache and config
        // are already children of the root directory
#ifndef WIN32
        if (fs::is_directory(legacy_config_dir)) {
            fs::copy(legacy_config_dir, config_dir, copy_options);
        }

        if (fs::is_directory(legacy_cache_dir)) {
            fs::copy(legacy_cache_dir, cache_dir, copy_options);
        }
#endif

        success_text.append(tr("\n\nIf you wish to clean up the files which were left in the old "
                               "data location, you can do so by deleting the following directory:\n"
                               "%1")
                                .arg(QString::fromStdString(legacy_user_dir.string())));
        break;
    }

    // Delete and re-create shader dir
    if (clear_shader_cache) {
        fs::remove_all(shader_dir);
        fs::create_directory(shader_dir);
    }

    emit finished(success_text, legacy_user_dir.string());
}
