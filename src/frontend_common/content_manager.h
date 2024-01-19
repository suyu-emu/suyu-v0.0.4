// SPDX-FileCopyrightText: 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <boost/algorithm/string.hpp>
#include "common/common_types.h"
#include "common/literals.h"
#include "core/core.h"
#include "core/file_sys/common_funcs.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/mode.h"
#include "core/file_sys/nca_metadata.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/submission_package.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/loader/loader.h"

namespace ContentManager {

enum class InstallResult {
    Success,
    Overwrite,
    Failure,
    BaseInstallAttempted,
};

/**
 * \brief Removes a single installed DLC
 * \param fs_controller [FileSystemController] reference from the Core::System instance
 * \param title_id Unique title ID representing the DLC which will be removed
 * \return 'true' if successful
 */
inline bool RemoveDLC(const Service::FileSystem::FileSystemController& fs_controller,
                      const u64 title_id) {
    return fs_controller.GetUserNANDContents()->RemoveExistingEntry(title_id) ||
           fs_controller.GetSDMCContents()->RemoveExistingEntry(title_id);
}

/**
 * \brief Removes all DLC for a game
 * \param system Raw pointer to the system instance
 * \param program_id Program ID for the game that will have all of its DLC removed
 * \return Number of DLC removed
 */
inline size_t RemoveAllDLC(Core::System* system, const u64 program_id) {
    size_t count{};
    const auto& fs_controller = system->GetFileSystemController();
    const auto dlc_entries = system->GetContentProvider().ListEntriesFilter(
        FileSys::TitleType::AOC, FileSys::ContentRecordType::Data);
    std::vector<u64> program_dlc_entries;

    for (const auto& entry : dlc_entries) {
        if (FileSys::GetBaseTitleID(entry.title_id) == program_id) {
            program_dlc_entries.push_back(entry.title_id);
        }
    }

    for (const auto& entry : program_dlc_entries) {
        if (RemoveDLC(fs_controller, entry)) {
            ++count;
        }
    }
    return count;
}

/**
 * \brief Removes the installed update for a game
 * \param fs_controller [FileSystemController] reference from the Core::System instance
 * \param program_id Program ID for the game that will have its installed update removed
 * \return 'true' if successful
 */
inline bool RemoveUpdate(const Service::FileSystem::FileSystemController& fs_controller,
                         const u64 program_id) {
    const auto update_id = program_id | 0x800;
    return fs_controller.GetUserNANDContents()->RemoveExistingEntry(update_id) ||
           fs_controller.GetSDMCContents()->RemoveExistingEntry(update_id);
}

/**
 * \brief Removes the base content for a game
 * \param fs_controller [FileSystemController] reference from the Core::System instance
 * \param program_id Program ID for the game that will have its base content removed
 * \return 'true' if successful
 */
inline bool RemoveBaseContent(const Service::FileSystem::FileSystemController& fs_controller,
                              const u64 program_id) {
    return fs_controller.GetUserNANDContents()->RemoveExistingEntry(program_id) ||
           fs_controller.GetSDMCContents()->RemoveExistingEntry(program_id);
}

/**
 * \brief Removes a mod for a game
 * \param fs_controller [FileSystemController] reference from the Core::System instance
 * \param program_id Program ID for the game where [mod_name] will be removed
 * \param mod_name The name of a mod as given by FileSys::PatchManager::GetPatches. This corresponds
 * with the name of the mod's directory in a game's load folder.
 * \return 'true' if successful
 */
inline bool RemoveMod(const Service::FileSystem::FileSystemController& fs_controller,
                      const u64 program_id, const std::string& mod_name) {
    // Check general Mods (LayeredFS and IPS)
    const auto mod_dir = fs_controller.GetModificationLoadRoot(program_id);
    if (mod_dir != nullptr) {
        return mod_dir->DeleteSubdirectoryRecursive(mod_name);
    }

    // Check SDMC mod directory (RomFS LayeredFS)
    const auto sdmc_mod_dir = fs_controller.GetSDMCModificationLoadRoot(program_id);
    if (sdmc_mod_dir != nullptr) {
        return sdmc_mod_dir->DeleteSubdirectoryRecursive(mod_name);
    }

    return false;
}

/**
 * \brief Installs an NSP
 * \param system Raw pointer to the system instance
 * \param vfs Raw pointer to the VfsFilesystem instance in Core::System
 * \param filename Path to the NSP file
 * \param callback Optional callback to report the progress of the installation. The first size_t
 * parameter is the total size of the virtual file and the second is the current progress. If you
 * return false to the callback, it will cancel the installation as soon as possible.
 * \return [InstallResult] representing how the installation finished
 */
inline InstallResult InstallNSP(
    Core::System* system, FileSys::VfsFilesystem* vfs, const std::string& filename,
    const std::function<bool(size_t, size_t)>& callback = std::function<bool(size_t, size_t)>()) {
    const auto copy = [callback](const FileSys::VirtualFile& src, const FileSys::VirtualFile& dest,
                                 std::size_t block_size) {
        if (src == nullptr || dest == nullptr) {
            return false;
        }
        if (!dest->Resize(src->GetSize())) {
            return false;
        }

        using namespace Common::Literals;
        std::vector<u8> buffer(1_MiB);

        for (std::size_t i = 0; i < src->GetSize(); i += buffer.size()) {
            if (callback(src->GetSize(), i)) {
                dest->Resize(0);
                return false;
            }
            const auto read = src->Read(buffer.data(), buffer.size(), i);
            dest->Write(buffer.data(), read, i);
        }
        return true;
    };

    std::shared_ptr<FileSys::NSP> nsp;
    FileSys::VirtualFile file = vfs->OpenFile(filename, FileSys::Mode::Read);
    if (boost::to_lower_copy(file->GetName()).ends_with(std::string("nsp"))) {
        nsp = std::make_shared<FileSys::NSP>(file);
        if (nsp->IsExtractedType()) {
            return InstallResult::Failure;
        }
    } else {
        return InstallResult::Failure;
    }

    if (nsp->GetStatus() != Loader::ResultStatus::Success) {
        return InstallResult::Failure;
    }
    const auto res =
        system->GetFileSystemController().GetUserNANDContents()->InstallEntry(*nsp, true, copy);
    switch (res) {
    case FileSys::InstallResult::Success:
        return InstallResult::Success;
    case FileSys::InstallResult::OverwriteExisting:
        return InstallResult::Overwrite;
    case FileSys::InstallResult::ErrorBaseInstall:
        return InstallResult::BaseInstallAttempted;
    default:
        return InstallResult::Failure;
    }
}

/**
 * \brief Installs an NCA
 * \param vfs Raw pointer to the VfsFilesystem instance in Core::System
 * \param filename Path to the NCA file
 * \param registered_cache Raw pointer to the registered cache that the NCA will be installed to
 * \param title_type Type of NCA package to install
 * \param callback Optional callback to report the progress of the installation. The first size_t
 * parameter is the total size of the virtual file and the second is the current progress. If you
 * return false to the callback, it will cancel the installation as soon as possible.
 * \return [InstallResult] representing how the installation finished
 */
inline InstallResult InstallNCA(
    FileSys::VfsFilesystem* vfs, const std::string& filename,
    FileSys::RegisteredCache* registered_cache, const FileSys::TitleType title_type,
    const std::function<bool(size_t, size_t)>& callback = std::function<bool(size_t, size_t)>()) {
    const auto copy = [callback](const FileSys::VirtualFile& src, const FileSys::VirtualFile& dest,
                                 std::size_t block_size) {
        if (src == nullptr || dest == nullptr) {
            return false;
        }
        if (!dest->Resize(src->GetSize())) {
            return false;
        }

        using namespace Common::Literals;
        std::vector<u8> buffer(1_MiB);

        for (std::size_t i = 0; i < src->GetSize(); i += buffer.size()) {
            if (callback(src->GetSize(), i)) {
                dest->Resize(0);
                return false;
            }
            const auto read = src->Read(buffer.data(), buffer.size(), i);
            dest->Write(buffer.data(), read, i);
        }
        return true;
    };

    const auto nca = std::make_shared<FileSys::NCA>(vfs->OpenFile(filename, FileSys::Mode::Read));
    const auto id = nca->GetStatus();

    // Game updates necessary are missing base RomFS
    if (id != Loader::ResultStatus::Success &&
        id != Loader::ResultStatus::ErrorMissingBKTRBaseRomFS) {
        return InstallResult::Failure;
    }

    const auto res = registered_cache->InstallEntry(*nca, title_type, true, copy);
    if (res == FileSys::InstallResult::Success) {
        return InstallResult::Success;
    } else if (res == FileSys::InstallResult::OverwriteExisting) {
        return InstallResult::Overwrite;
    } else {
        return InstallResult::Failure;
    }
}

} // namespace ContentManager
