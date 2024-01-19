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

inline bool RemoveDLC(const Service::FileSystem::FileSystemController& fs_controller,
                      const u64 title_id) {
    return fs_controller.GetUserNANDContents()->RemoveExistingEntry(title_id) ||
           fs_controller.GetSDMCContents()->RemoveExistingEntry(title_id);
}

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

inline bool RemoveUpdate(const Service::FileSystem::FileSystemController& fs_controller,
                         const u64 program_id) {
    const auto update_id = program_id | 0x800;
    return fs_controller.GetUserNANDContents()->RemoveExistingEntry(update_id) ||
           fs_controller.GetSDMCContents()->RemoveExistingEntry(update_id);
}

inline bool RemoveBaseContent(const Service::FileSystem::FileSystemController& fs_controller,
                              const u64 program_id) {
    return fs_controller.GetUserNANDContents()->RemoveExistingEntry(program_id) ||
           fs_controller.GetSDMCContents()->RemoveExistingEntry(program_id);
}

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
