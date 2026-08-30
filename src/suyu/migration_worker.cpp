// SPDX-FileCopyrightText: Copyright 2025 suyu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "migration_worker.h"
#include "common/fs/symlink.h"

#include <array>
#include <QMap>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <filesystem>

#include "common/fs/path_util.h"

std::array<Emulator, 4> BuildLegacyEmulators() {
#ifdef _WIN32
    const auto roaming = Common::FS::GetAppDataRoamingDirectory();
    return {
        Emulator{QT_TR_NOOP("Citron"), roaming / "Citron", roaming / "Citron", roaming / "Citron"},
        Emulator{QT_TR_NOOP("Sudachi"), roaming / "Sudachi", roaming / "Sudachi", roaming / "Sudachi"},
        Emulator{QT_TR_NOOP("Suyu"), roaming / "suyu", roaming / "suyu", roaming / "suyu"},
        Emulator{QT_TR_NOOP("Yuzu"), roaming / "yuzu", roaming / "yuzu", roaming / "yuzu"},
    };
#else
    const auto home = Common::FS::GetHomeDirectory();
    const auto share = home / ".local" / "share";
    const auto config = home / ".config";
    const auto cache = home / ".cache";
    return {
        Emulator{QT_TR_NOOP("Citron"), share / "citron", config / "citron", cache / "citron"},
        Emulator{QT_TR_NOOP("Sudachi"), share / "sudachi", config / "sudachi", cache / "sudachi"},
        Emulator{QT_TR_NOOP("Suyu"), share / "suyu", config / "suyu", cache / "suyu"},
        Emulator{QT_TR_NOOP("Yuzu"), share / "yuzu", config / "yuzu", cache / "yuzu"},
    };
#endif
}

const std::array<Emulator, 4> legacy_emus = BuildLegacyEmulators();

MigrationWorker::MigrationWorker(const Emulator selected_legacy_emu_,
                                 const bool clear_shader_cache_,
                                 const MigrationStrategy strategy_)
    : QObject()
    , selected_legacy_emu(selected_legacy_emu_)
    , clear_shader_cache(clear_shader_cache_)
    , strategy(strategy_)
{}

void MigrationWorker::process()
{
    namespace fs = std::filesystem;
    constexpr auto copy_options = fs::copy_options::update_existing | fs::copy_options::recursive;

    const fs::path legacy_user_dir = selected_legacy_emu.get_user_dir();
    const fs::path legacy_config_dir = selected_legacy_emu.get_config_dir();
    const fs::path legacy_cache_dir = selected_legacy_emu.get_cache_dir();

    // TODO(crueter): Make these constexpr since they're defaulted
    const fs::path suyu_dir = Common::FS::GetSuyuPath(Common::FS::SuyuPath::SuyuDir);
    const fs::path config_dir = Common::FS::GetSuyuPath(Common::FS::SuyuPath::ConfigDir);
    const fs::path cache_dir = Common::FS::GetSuyuPath(Common::FS::SuyuPath::CacheDir);
    const fs::path shader_dir = Common::FS::GetSuyuPath(Common::FS::SuyuPath::ShaderDir);

    try {
        fs::remove_all(suyu_dir);
    } catch (const fs::filesystem_error&) {
        // ignore because linux does stupid crap sometimes
    }

    switch (strategy) {
    case MigrationStrategy::Link:
        // Create symlinks/directory junctions if requested

        // Windows 11 has random permission nonsense to deal with.
        try {
            Common::FS::CreateSymlink(legacy_user_dir, suyu_dir);
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
                                .arg(selected_legacy_emu.name(),
                                     QString::fromStdString(suyu_dir.string()),
                                     QString::fromStdString(config_dir.string()),
                                     QString::fromStdString(cache_dir.string())));

        break;
    case MigrationStrategy::Move:
        // Rename directories if deletion is requested (achieves the same result)
        fs::rename(legacy_user_dir, suyu_dir);

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
        fs::copy(legacy_user_dir, suyu_dir, copy_options);

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
