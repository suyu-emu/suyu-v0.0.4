// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <filesystem>
#include <fmt/format.h>
#include "common/fs/fs.h"
#include "common/fs/fs_types.h"
#include "common/logging/backend.h"
#include "frontend_common/data_manager.h"
#include "mod_manager.h"

namespace FrontendCommon {

// TODO: Handle cases where the folder appears to contain multiple mods.
std::vector<std::filesystem::path> GetModFolder(const std::string& root) {
    std::vector<std::filesystem::path> paths;

    auto callback = [&paths](const std::filesystem::directory_entry& entry) -> bool {
        const auto name = entry.path().filename().string();
        static const std::array<std::string, 5> valid_names = {"exefs",
                                                               "romfs",
                                                               "romfs_ext",
                                                               "cheats",
                                                               "romfslite"};

        if (std::ranges::find(valid_names, name) != valid_names.end()) {
            paths.emplace_back(entry.path().parent_path());
        }

        return true;
    };

    Common::FS::IterateDirEntriesRecursively(root, callback, Common::FS::DirEntryFilter::Directory);

    return paths;
}

ModInstallResult InstallMod(const std::filesystem::path& path, const u64 program_id, const bool copy) {
    const auto program_id_string = fmt::format("{:016X}", program_id);
    const auto mod_name = path.filename();
    const auto mod_dir =
        DataManager::GetDataDir(DataManager::DataDir::Mods) / program_id_string / mod_name;

    // pre-emptively remove any existing mod here
    std::filesystem::remove_all(mod_dir);

    // now copy
    try {
        std::filesystem::copy(path, mod_dir, std::filesystem::copy_options::recursive);
        if (!copy)
            std::filesystem::remove_all(path);
    } catch (std::exception& e) {
        LOG_ERROR(Frontend, "Mod install failed with message {}", e.what());
        return Failed;
    }

    LOG_INFO(Frontend, "Copied mod from {} to {}", path.string(), mod_dir.string());

    return Success;
}

} // namespace FrontendCommon
