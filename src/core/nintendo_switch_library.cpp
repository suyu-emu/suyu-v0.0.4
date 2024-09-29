// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <string>
#include <vector>

#include "common/logging/log.h"
#include "core/core.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/registered_cache.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/loader/loader.h"
#include "core/nintendo_switch_library.h"

namespace Core {

/**
 * NintendoSwitchLibrary class manages the operations related to installed games
 * on the emulated Nintendo Switch, including listing games and launching them.
 */
class NintendoSwitchLibrary {
public:
    explicit NintendoSwitchLibrary(Core::System& system) : system(system) {}

    struct GameInfo {
        u64 program_id;
        std::string title_name;
        std::string file_path;
    };

    [[nodiscard]] std::vector<GameInfo> GetInstalledGames() {
        std::vector<GameInfo> games;
        const auto& cache = system.GetContentProvider().GetUserNANDCache();

        for (const auto& [program_id, content_type] : cache.GetAllEntries()) {
            if (content_type == FileSys::ContentRecordType::Program) {
                const auto title_name = GetGameName(program_id);
                const auto file_path = cache.GetEntryUnparsed(program_id, FileSys::ContentRecordType::Program);

                if (!title_name.empty() && !file_path.empty()) {
                    games.push_back({program_id, title_name, file_path});
                }
            }
        }

        return games;
    }

    [[nodiscard]] std::string GetGameName(u64 program_id) {
        const auto& patch_manager = system.GetFileSystemController().GetPatchManager(program_id);
        const auto metadata = patch_manager.GetControlMetadata();

        if (metadata.first != nullptr) {
            return metadata.first->GetApplicationName();
        }

        return "";
    }

    [[nodiscard]] bool LaunchGame(u64 program_id) {
        const auto file_path = system.GetContentProvider().GetUserNANDCache().GetEntryUnparsed(program_id, FileSys::ContentRecordType::Program);

        if (file_path.empty()) {
            LOG_ERROR(Core, "Failed to launch game. File not found for program_id={:016X}", program_id);
            return false;
        }

        const auto loader = Loader::GetLoader(system, file_path);
        if (!loader) {
            LOG_ERROR(Core, "Failed to create loader for game. program_id={:016X}", program_id);
            return false;
        }

        const auto result = system.Load(*loader);
        if (result != ResultStatus::Success) {
            LOG_ERROR(Core, "Failed to load game. Error: {}, program_id={:016X}", result, program_id);
            return false;
        }

        LOG_INFO(Core, "Successfully launched game. program_id={:016X}", program_id);
        return true;
    }

private:
    const Core::System& system;
};

} // namespace Core