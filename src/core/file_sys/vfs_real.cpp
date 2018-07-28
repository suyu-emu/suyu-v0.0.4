// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <utility>

#include "common/common_paths.h"
#include "common/logging/log.h"
#include "core/file_sys/vfs_real.h"

namespace FileSys {

static std::string ModeFlagsToString(Mode mode) {
    std::string mode_str;

    // Calculate the correct open mode for the file.
    if (mode & Mode::Read && mode & Mode::Write) {
        if (mode & Mode::Append)
            mode_str = "a+";
        else
            mode_str = "r+";
    } else {
        if (mode & Mode::Read)
            mode_str = "r";
        else if (mode & Mode::Append)
            mode_str = "a";
        else if (mode & Mode::Write)
            mode_str = "w";
    }

    mode_str += "b";

    return mode_str;
}

RealVfsFile::RealVfsFile(const std::string& path_, Mode perms_)
    : backing(path_, ModeFlagsToString(perms_).c_str()), path(path_),
      parent_path(FileUtil::GetParentPath(path_)),
      path_components(FileUtil::SplitPathComponents(path_)),
      parent_components(FileUtil::SliceVector(path_components, 0, path_components.size() - 1)),
      perms(perms_) {}

std::string RealVfsFile::GetName() const {
    return path_components.back();
}

size_t RealVfsFile::GetSize() const {
    return backing.GetSize();
}

bool RealVfsFile::Resize(size_t new_size) {
    return backing.Resize(new_size);
}

std::shared_ptr<VfsDirectory> RealVfsFile::GetContainingDirectory() const {
    return std::make_shared<RealVfsDirectory>(parent_path, perms);
}

bool RealVfsFile::IsWritable() const {
    return (perms & Mode::WriteAppend) != 0;
}

bool RealVfsFile::IsReadable() const {
    return (perms & Mode::ReadWrite) != 0;
}

size_t RealVfsFile::Read(u8* data, size_t length, size_t offset) const {
    if (!backing.Seek(offset, SEEK_SET))
        return 0;
    return backing.ReadBytes(data, length);
}

size_t RealVfsFile::Write(const u8* data, size_t length, size_t offset) {
    if (!backing.Seek(offset, SEEK_SET))
        return 0;
    return backing.WriteBytes(data, length);
}

bool RealVfsFile::Rename(std::string_view name) {
    std::string name_str(name.begin(), name.end());
    const auto out = FileUtil::Rename(GetName(), name_str);

    path = (parent_path + DIR_SEP).append(name);
    path_components = parent_components;
    path_components.push_back(std::move(name_str));
    backing = FileUtil::IOFile(path, ModeFlagsToString(perms).c_str());

    return out;
}

bool RealVfsFile::Close() {
    return backing.Close();
}

RealVfsDirectory::RealVfsDirectory(const std::string& path_, Mode perms_)
    : path(FileUtil::RemoveTrailingSlash(path_)), parent_path(FileUtil::GetParentPath(path)),
      path_components(FileUtil::SplitPathComponents(path)),
      parent_components(FileUtil::SliceVector(path_components, 0, path_components.size() - 1)),
      perms(perms_) {
    if (!FileUtil::Exists(path) && perms & Mode::WriteAppend)
        FileUtil::CreateDir(path);

    if (perms == Mode::Append)
        return;

    FileUtil::ForeachDirectoryEntry(
        nullptr, path,
        [this](u64* entries_out, const std::string& directory, const std::string& filename) {
            std::string full_path = directory + DIR_SEP + filename;
            if (FileUtil::IsDirectory(full_path))
                subdirectories.emplace_back(std::make_shared<RealVfsDirectory>(full_path, perms));
            else
                files.emplace_back(std::make_shared<RealVfsFile>(full_path, perms));
            return true;
        });
}

std::vector<std::shared_ptr<VfsFile>> RealVfsDirectory::GetFiles() const {
    return files;
}

std::vector<std::shared_ptr<VfsDirectory>> RealVfsDirectory::GetSubdirectories() const {
    return subdirectories;
}

bool RealVfsDirectory::IsWritable() const {
    return (perms & Mode::WriteAppend) != 0;
}

bool RealVfsDirectory::IsReadable() const {
    return (perms & Mode::ReadWrite) != 0;
}

std::string RealVfsDirectory::GetName() const {
    return path_components.back();
}

std::shared_ptr<VfsDirectory> RealVfsDirectory::GetParentDirectory() const {
    if (path_components.size() <= 1)
        return nullptr;

    return std::make_shared<RealVfsDirectory>(parent_path, perms);
}

std::shared_ptr<VfsDirectory> RealVfsDirectory::CreateSubdirectory(std::string_view name) {
    const std::string subdir_path = (path + DIR_SEP).append(name);

    if (!FileUtil::CreateDir(subdir_path)) {
        return nullptr;
    }

    subdirectories.emplace_back(std::make_shared<RealVfsDirectory>(subdir_path, perms));
    return subdirectories.back();
}

std::shared_ptr<VfsFile> RealVfsDirectory::CreateFile(std::string_view name) {
    const std::string file_path = (path + DIR_SEP).append(name);

    if (!FileUtil::CreateEmptyFile(file_path)) {
        return nullptr;
    }

    files.emplace_back(std::make_shared<RealVfsFile>(file_path, perms));
    return files.back();
}

bool RealVfsDirectory::DeleteSubdirectory(std::string_view name) {
    const std::string subdir_path = (path + DIR_SEP).append(name);

    return FileUtil::DeleteDirRecursively(subdir_path);
}

bool RealVfsDirectory::DeleteFile(std::string_view name) {
    const auto file = GetFile(name);

    if (file == nullptr) {
        return false;
    }

    files.erase(std::find(files.begin(), files.end(), file));

    auto real_file = std::static_pointer_cast<RealVfsFile>(file);
    real_file->Close();

    const std::string file_path = (path + DIR_SEP).append(name);
    return FileUtil::Delete(file_path);
}

bool RealVfsDirectory::Rename(std::string_view name) {
    const std::string new_name = (parent_path + DIR_SEP).append(name);

    return FileUtil::Rename(path, new_name);
}

std::string RealVfsDirectory::GetFullPath() const {
    auto out = path;
    std::replace(out.begin(), out.end(), '\\', '/');
    return out;
}

bool RealVfsDirectory::ReplaceFileWithSubdirectory(VirtualFile file, VirtualDir dir) {
    const auto iter = std::find(files.begin(), files.end(), file);
    if (iter == files.end())
        return false;

    const std::ptrdiff_t offset = std::distance(files.begin(), iter);
    files[offset] = files.back();
    files.pop_back();

    subdirectories.emplace_back(std::move(dir));

    return true;
}
} // namespace FileSys
