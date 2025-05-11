#include "migration_worker.h"

#include <filesystem>

#include "common/fs/path_util.h"

MigrationWorker::MigrationWorker(const LegacyEmu selected_legacy_emu_,
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
    const auto copy_options = fs::copy_options::update_existing | fs::copy_options::recursive;

    std::string legacy_user_dir;
    std::string legacy_config_dir;
    std::string legacy_cache_dir;

#define LEGACY_EMU(emu) \
    case LegacyEmu::emu: \
        legacy_user_dir = Common::FS::GetLegacyPath(Common::FS::LegacyPath::emu##Dir).string(); \
        legacy_config_dir = Common::FS::GetLegacyPath(Common::FS::LegacyPath::emu##ConfigDir) \
                                .string(); \
        legacy_cache_dir = Common::FS::GetLegacyPath(Common::FS::LegacyPath::emu##CacheDir) \
                               .string(); \
        break;

    switch (selected_legacy_emu) {
        LEGACY_EMU(Citron)
        LEGACY_EMU(Sudachi)
        LEGACY_EMU(Yuzu)
        LEGACY_EMU(Suyu)
    }

#undef LEGACY_EMU

    fs::path eden_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::EdenDir);
    fs::path config_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::ConfigDir);
    fs::path cache_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir);
    fs::path shader_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::ShaderDir);

    try {
        fs::remove_all(eden_dir);
    } catch (fs::filesystem_error &_) {
        // ignore because linux does stupid crap sometimes.
    }

    switch (strategy) {
    case MigrationStrategy::Link:
        // Create symlinks/directory junctions if requested

        // Windows 11 has random permission nonsense to deal with.
        try {
            fs::create_directory_symlink(legacy_user_dir, eden_dir);
        } catch (const fs::filesystem_error &e) {
            emit error(tr("Linking the old directory failed. You may need to re-run with "
                          "administrative privileges on Windows.\nOS gave error: %1")
                           .arg(tr(e.what())));
            return;
        }

// Windows doesn't need any more links, because cache and config
// are already children of the root directory
#ifndef WIN32
        if (fs::is_directory(legacy_config_dir)) {
            fs::create_directory_symlink(legacy_config_dir, config_dir);
        }

        if (fs::is_directory(legacy_cache_dir)) {
            fs::create_directory_symlink(legacy_cache_dir, cache_dir);
        }
#endif
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

        if (fs::is_directory(legacy_config_dir)) {
            fs::copy(legacy_config_dir, config_dir, copy_options);
        }

        if (fs::is_directory(legacy_cache_dir)) {
            fs::copy(legacy_cache_dir, cache_dir, copy_options);
        }

        success_text.append(tr("\n\nIf you wish to clean up the files which were left in the old "
                               "data location, you can do so by deleting the following directory:\n"
                               "%1")
                                .arg(QString::fromStdString(legacy_user_dir)));
        break;
    }

    // Delete and re-create shader dir
    if (clear_shader_cache) {
        fs::remove_all(shader_dir);
        fs::create_directory(shader_dir);
    }

    emit finished(success_text);
}
