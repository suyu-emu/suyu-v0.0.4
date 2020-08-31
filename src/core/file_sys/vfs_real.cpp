// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <utility>
#include "common/assert.h"
#include "common/common_paths.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/file_sys/vfs_real.h"

namespace FileSys {

namespace FS = Common::FS;

static std::string ModeFlagsToString(Mode mode) {
    std::string mode_str;

    // Calculate the correct open mode for the file.
    if (True(mode & Mode::Read) && True(mode & Mode::Write)) {
        if (True(mode & Mode::Append)) {
            mode_str = "a+";
        } else {
            mode_str = "r+";
        }
    } else {
        if (True(mode & Mode::Read)) {
            mode_str = "r";
        } else if (True(mode & Mode::Append)) {
            mode_str = "a";
        } else if (True(mode & Mode::Write)) {
            mode_str = "w";
        } else {
            UNREACHABLE_MSG("Invalid file open mode: {:02X}", static_cast<u8>(mode));
        }
    }

    mode_str += "b";

    return mode_str;
}

RealVfsFilesystem::RealVfsFilesystem() : VfsFilesystem(nullptr) {}
RealVfsFilesystem::~RealVfsFilesystem() = default;

std::string RealVfsFilesystem::GetName() const {
    return "Real";
}

bool RealVfsFilesystem::IsReadable() const {
    return true;
}

bool RealVfsFilesystem::IsWritable() const {
    return true;
}

VfsEntryType RealVfsFilesystem::GetEntryType(std::string_view path_) const {
    const auto path = FS::SanitizePath(path_, FS::DirectorySeparator::PlatformDefault);
    if (!FS::Exists(path)) {
        return VfsEntryType::None;
    }
    if (FS::IsDirectory(path)) {
        return VfsEntryType::Directory;
    }

    return VfsEntryType::File;
}

VirtualFile RealVfsFilesystem::OpenFile(std::string_view path_, Mode perms) {
    const auto path = FS::SanitizePath(path_, FS::DirectorySeparator::PlatformDefault);

    if (const auto weak_iter = cache.find(path); weak_iter != cache.cend()) {
        const auto& weak = weak_iter->second;

        if (!weak.expired()) {
            return std::shared_ptr<RealVfsFile>(new RealVfsFile(*this, weak.lock(), path, perms));
        }
    }

    if (!FS::Exists(path) && True(perms & Mode::WriteAppend)) {
        FS::CreateEmptyFile(path);
    }

    auto backing = std::make_shared<FS::IOFile>(path, ModeFlagsToString(perms).c_str());
    cache.insert_or_assign(path, backing);

    // Cannot use make_shared as RealVfsFile constructor is private
    return std::shared_ptr<RealVfsFile>(new RealVfsFile(*this, backing, path, perms));
}

VirtualFile RealVfsFilesystem::CreateFile(std::string_view path_, Mode perms) {
    const auto path = FS::SanitizePath(path_, FS::DirectorySeparator::PlatformDefault);
    const auto path_fwd = FS::SanitizePath(path, FS::DirectorySeparator::ForwardSlash);
    if (!FS::Exists(path)) {
        FS::CreateFullPath(path_fwd);
        if (!FS::CreateEmptyFile(path)) {
            return nullptr;
        }
    }
    return OpenFile(path, perms);
}

VirtualFile RealVfsFilesystem::CopyFile(std::string_view old_path_, std::string_view new_path_) {
    const auto old_path = FS::SanitizePath(old_path_, FS::DirectorySeparator::PlatformDefault);
    const auto new_path = FS::SanitizePath(new_path_, FS::DirectorySeparator::PlatformDefault);

    if (!FS::Exists(old_path) || FS::Exists(new_path) || FS::IsDirectory(old_path) ||
        !FS::Copy(old_path, new_path)) {
        return nullptr;
    }
    return OpenFile(new_path, Mode::ReadWrite);
}

VirtualFile RealVfsFilesystem::MoveFile(std::string_view old_path_, std::string_view new_path_) {
    const auto old_path = FS::SanitizePath(old_path_, FS::DirectorySeparator::PlatformDefault);
    const auto new_path = FS::SanitizePath(new_path_, FS::DirectorySeparator::PlatformDefault);
    const auto cached_file_iter = cache.find(old_path);

    if (cached_file_iter != cache.cend()) {
        auto file = cached_file_iter->second.lock();

        if (!cached_file_iter->second.expired()) {
            file->Close();
        }

        if (!FS::Exists(old_path) || FS::Exists(new_path) || FS::IsDirectory(old_path) ||
            !FS::Rename(old_path, new_path)) {
            return nullptr;
        }

        cache.erase(old_path);
        if (file->Open(new_path, "r+b")) {
            cache.insert_or_assign(new_path, std::move(file));
        } else {
            LOG_ERROR(Service_FS, "Failed to open path {} in order to re-cache it", new_path);
        }
    } else {
        UNREACHABLE();
        return nullptr;
    }

    return OpenFile(new_path, Mode::ReadWrite);
}

bool RealVfsFilesystem::DeleteFile(std::string_view path_) {
    const auto path = FS::SanitizePath(path_, FS::DirectorySeparator::PlatformDefault);
    const auto cached_iter = cache.find(path);

    if (cached_iter != cache.cend()) {
        if (!cached_iter->second.expired()) {
            cached_iter->second.lock()->Close();
        }
        cache.erase(path);
    }

    return FS::Delete(path);
}

VirtualDir RealVfsFilesystem::OpenDirectory(std::string_view path_, Mode perms) {
    const auto path = FS::SanitizePath(path_, FS::DirectorySeparator::PlatformDefault);
    // Cannot use make_shared as RealVfsDirectory constructor is private
    return std::shared_ptr<RealVfsDirectory>(new RealVfsDirectory(*this, path, perms));
}

VirtualDir RealVfsFilesystem::CreateDirectory(std::string_view path_, Mode perms) {
    const auto path = FS::SanitizePath(path_, FS::DirectorySeparator::PlatformDefault);
    const auto path_fwd = FS::SanitizePath(path, FS::DirectorySeparator::ForwardSlash);
    if (!FS::Exists(path)) {
        FS::CreateFullPath(path_fwd);
        if (!FS::CreateDir(path)) {
            return nullptr;
        }
    }
    // Cannot use make_shared as RealVfsDirectory constructor is private
    return std::shared_ptr<RealVfsDirectory>(new RealVfsDirectory(*this, path, perms));
}

VirtualDir RealVfsFilesystem::CopyDirectory(std::string_view old_path_,
                                            std::string_view new_path_) {
    const auto old_path = FS::SanitizePath(old_path_, FS::DirectorySeparator::PlatformDefault);
    const auto new_path = FS::SanitizePath(new_path_, FS::DirectorySeparator::PlatformDefault);
    if (!FS::Exists(old_path) || FS::Exists(new_path) || !FS::IsDirectory(old_path)) {
        return nullptr;
    }
    FS::CopyDir(old_path, new_path);
    return OpenDirectory(new_path, Mode::ReadWrite);
}

VirtualDir RealVfsFilesystem::MoveDirectory(std::string_view old_path_,
                                            std::string_view new_path_) {
    const auto old_path = FS::SanitizePath(old_path_, FS::DirectorySeparator::PlatformDefault);
    const auto new_path = FS::SanitizePath(new_path_, FS::DirectorySeparator::PlatformDefault);

    if (!FS::Exists(old_path) || FS::Exists(new_path) || FS::IsDirectory(old_path) ||
        !FS::Rename(old_path, new_path)) {
        return nullptr;
    }

    for (auto& kv : cache) {
        // If the path in the cache doesn't start with old_path, then bail on this file.
        if (kv.first.rfind(old_path, 0) != 0) {
            continue;
        }

        const auto file_old_path =
            FS::SanitizePath(kv.first, FS::DirectorySeparator::PlatformDefault);
        auto file_new_path = FS::SanitizePath(new_path + DIR_SEP + kv.first.substr(old_path.size()),
                                              FS::DirectorySeparator::PlatformDefault);
        const auto& cached = cache[file_old_path];

        if (cached.expired()) {
            continue;
        }

        auto file = cached.lock();
        cache.erase(file_old_path);
        if (file->Open(file_new_path, "r+b")) {
            cache.insert_or_assign(std::move(file_new_path), std::move(file));
        } else {
            LOG_ERROR(Service_FS, "Failed to open path {} in order to re-cache it", file_new_path);
        }
    }

    return OpenDirectory(new_path, Mode::ReadWrite);
}

bool RealVfsFilesystem::DeleteDirectory(std::string_view path_) {
    const auto path = FS::SanitizePath(path_, FS::DirectorySeparator::PlatformDefault);

    for (auto& kv : cache) {
        // If the path in the cache doesn't start with path, then bail on this file.
        if (kv.first.rfind(path, 0) != 0) {
            continue;
        }

        const auto& entry = cache[kv.first];
        if (!entry.expired()) {
            entry.lock()->Close();
        }

        cache.erase(kv.first);
    }

    return FS::DeleteDirRecursively(path);
}

RealVfsFile::RealVfsFile(RealVfsFilesystem& base_, std::shared_ptr<FS::IOFile> backing_,
                         const std::string& path_, Mode perms_)
    : base(base_), backing(std::move(backing_)), path(path_), parent_path(FS::GetParentPath(path_)),
      path_components(FS::SplitPathComponents(path_)),
      parent_components(FS::SliceVector(path_components, 0, path_components.size() - 1)),
      perms(perms_) {}

RealVfsFile::~RealVfsFile() = default;

std::string RealVfsFile::GetName() const {
    return path_components.back();
}

std::size_t RealVfsFile::GetSize() const {
    return backing->GetSize();
}

bool RealVfsFile::Resize(std::size_t new_size) {
    return backing->Resize(new_size);
}

VirtualDir RealVfsFile::GetContainingDirectory() const {
    return base.OpenDirectory(parent_path, perms);
}

bool RealVfsFile::IsWritable() const {
    return True(perms & Mode::WriteAppend);
}

bool RealVfsFile::IsReadable() const {
    return True(perms & Mode::ReadWrite);
}

std::size_t RealVfsFile::Read(u8* data, std::size_t length, std::size_t offset) const {
    if (!backing->Seek(static_cast<s64>(offset), SEEK_SET)) {
        return 0;
    }
    return backing->ReadBytes(data, length);
}

std::size_t RealVfsFile::Write(const u8* data, std::size_t length, std::size_t offset) {
    if (!backing->Seek(static_cast<s64>(offset), SEEK_SET)) {
        return 0;
    }
    return backing->WriteBytes(data, length);
}

bool RealVfsFile::Rename(std::string_view name) {
    return base.MoveFile(path, parent_path + DIR_SEP + std::string(name)) != nullptr;
}

bool RealVfsFile::Close() {
    return backing->Close();
}

// TODO(DarkLordZach): MSVC would not let me combine the following two functions using 'if
// constexpr' because there is a compile error in the branch not used.

template <>
std::vector<VirtualFile> RealVfsDirectory::IterateEntries<RealVfsFile, VfsFile>() const {
    if (perms == Mode::Append) {
        return {};
    }

    std::vector<VirtualFile> out;
    FS::ForeachDirectoryEntry(
        nullptr, path,
        [&out, this](u64* entries_out, const std::string& directory, const std::string& filename) {
            const std::string full_path = directory + DIR_SEP + filename;
            if (!FS::IsDirectory(full_path)) {
                out.emplace_back(base.OpenFile(full_path, perms));
            }
            return true;
        });

    return out;
}

template <>
std::vector<VirtualDir> RealVfsDirectory::IterateEntries<RealVfsDirectory, VfsDirectory>() const {
    if (perms == Mode::Append) {
        return {};
    }

    std::vector<VirtualDir> out;
    FS::ForeachDirectoryEntry(
        nullptr, path,
        [&out, this](u64* entries_out, const std::string& directory, const std::string& filename) {
            const std::string full_path = directory + DIR_SEP + filename;
            if (FS::IsDirectory(full_path)) {
                out.emplace_back(base.OpenDirectory(full_path, perms));
            }
            return true;
        });

    return out;
}

RealVfsDirectory::RealVfsDirectory(RealVfsFilesystem& base_, const std::string& path_, Mode perms_)
    : base(base_), path(FS::RemoveTrailingSlash(path_)), parent_path(FS::GetParentPath(path)),
      path_components(FS::SplitPathComponents(path)),
      parent_components(FS::SliceVector(path_components, 0, path_components.size() - 1)),
      perms(perms_) {
    if (!FS::Exists(path) && True(perms & Mode::WriteAppend)) {
        FS::CreateDir(path);
    }
}

RealVfsDirectory::~RealVfsDirectory() = default;

VirtualFile RealVfsDirectory::GetFileRelative(std::string_view path) const {
    const auto full_path = FS::SanitizePath(this->path + DIR_SEP + std::string(path));
    if (!FS::Exists(full_path) || FS::IsDirectory(full_path)) {
        return nullptr;
    }
    return base.OpenFile(full_path, perms);
}

VirtualDir RealVfsDirectory::GetDirectoryRelative(std::string_view path) const {
    const auto full_path = FS::SanitizePath(this->path + DIR_SEP + std::string(path));
    if (!FS::Exists(full_path) || !FS::IsDirectory(full_path)) {
        return nullptr;
    }
    return base.OpenDirectory(full_path, perms);
}

VirtualFile RealVfsDirectory::GetFile(std::string_view name) const {
    return GetFileRelative(name);
}

VirtualDir RealVfsDirectory::GetSubdirectory(std::string_view name) const {
    return GetDirectoryRelative(name);
}

VirtualFile RealVfsDirectory::CreateFileRelative(std::string_view path) {
    const auto full_path = FS::SanitizePath(this->path + DIR_SEP + std::string(path));
    return base.CreateFile(full_path, perms);
}

VirtualDir RealVfsDirectory::CreateDirectoryRelative(std::string_view path) {
    const auto full_path = FS::SanitizePath(this->path + DIR_SEP + std::string(path));
    return base.CreateDirectory(full_path, perms);
}

bool RealVfsDirectory::DeleteSubdirectoryRecursive(std::string_view name) {
    const auto full_path = FS::SanitizePath(this->path + DIR_SEP + std::string(name));
    return base.DeleteDirectory(full_path);
}

std::vector<VirtualFile> RealVfsDirectory::GetFiles() const {
    return IterateEntries<RealVfsFile, VfsFile>();
}

std::vector<VirtualDir> RealVfsDirectory::GetSubdirectories() const {
    return IterateEntries<RealVfsDirectory, VfsDirectory>();
}

bool RealVfsDirectory::IsWritable() const {
    return True(perms & Mode::WriteAppend);
}

bool RealVfsDirectory::IsReadable() const {
    return True(perms & Mode::ReadWrite);
}

std::string RealVfsDirectory::GetName() const {
    return path_components.back();
}

VirtualDir RealVfsDirectory::GetParentDirectory() const {
    if (path_components.size() <= 1) {
        return nullptr;
    }

    return base.OpenDirectory(parent_path, perms);
}

VirtualDir RealVfsDirectory::CreateSubdirectory(std::string_view name) {
    const std::string subdir_path = (path + DIR_SEP).append(name);
    return base.CreateDirectory(subdir_path, perms);
}

VirtualFile RealVfsDirectory::CreateFile(std::string_view name) {
    const std::string file_path = (path + DIR_SEP).append(name);
    return base.CreateFile(file_path, perms);
}

bool RealVfsDirectory::DeleteSubdirectory(std::string_view name) {
    const std::string subdir_path = (path + DIR_SEP).append(name);
    return base.DeleteDirectory(subdir_path);
}

bool RealVfsDirectory::DeleteFile(std::string_view name) {
    const std::string file_path = (path + DIR_SEP).append(name);
    return base.DeleteFile(file_path);
}

bool RealVfsDirectory::Rename(std::string_view name) {
    const std::string new_name = (parent_path + DIR_SEP).append(name);
    return base.MoveFile(path, new_name) != nullptr;
}

std::string RealVfsDirectory::GetFullPath() const {
    auto out = path;
    std::replace(out.begin(), out.end(), '\\', '/');
    return out;
}

std::map<std::string, VfsEntryType, std::less<>> RealVfsDirectory::GetEntries() const {
    if (perms == Mode::Append) {
        return {};
    }

    std::map<std::string, VfsEntryType, std::less<>> out;
    FS::ForeachDirectoryEntry(
        nullptr, path,
        [&out](u64* entries_out, const std::string& directory, const std::string& filename) {
            const std::string full_path = directory + DIR_SEP + filename;
            out.emplace(filename,
                        FS::IsDirectory(full_path) ? VfsEntryType::Directory : VfsEntryType::File);
            return true;
        });

    return out;
}

} // namespace FileSys
