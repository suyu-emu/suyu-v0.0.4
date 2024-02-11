// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/common_funcs.h"
#include "core/file_sys/fsa/fs_i_filesystem.h"
#include "core/file_sys/vfs/vfs.h"
#include "core/hle/service/cmif_types.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/hle/service/filesystem/fsp/fsp_util.h"
#include "core/hle/service/service.h"

namespace FileSys::Sf {
struct Path;
}

namespace Service::FileSystem {

class IFile;
class IDirectory;

class IFileSystem final : public ServiceFramework<IFileSystem> {
public:
    explicit IFileSystem(Core::System& system_, FileSys::VirtualDir dir_, SizeGetter size_getter_);

    struct FileSystemAttribute {
        u8 dir_entry_name_length_max_defined;
        u8 file_entry_name_length_max_defined;
        u8 dir_path_name_length_max_defined;
        u8 file_path_name_length_max_defined;
        INSERT_PADDING_BYTES_NOINIT(0x5);
        u8 utf16_dir_entry_name_length_max_defined;
        u8 utf16_file_entry_name_length_max_defined;
        u8 utf16_dir_path_name_length_max_defined;
        u8 utf16_file_path_name_length_max_defined;
        INSERT_PADDING_BYTES_NOINIT(0x18);
        s32 dir_entry_name_length_max;
        s32 file_entry_name_length_max;
        s32 dir_path_name_length_max;
        s32 file_path_name_length_max;
        INSERT_PADDING_WORDS_NOINIT(0x5);
        s32 utf16_dir_entry_name_length_max;
        s32 utf16_file_entry_name_length_max;
        s32 utf16_dir_path_name_length_max;
        s32 utf16_file_path_name_length_max;
        INSERT_PADDING_WORDS_NOINIT(0x18);
        INSERT_PADDING_WORDS_NOINIT(0x1);
    };
    static_assert(sizeof(FileSystemAttribute) == 0xC0, "FileSystemAttribute has incorrect size");

    Result CreateFile(const InLargeData<FileSys::Sf::Path, BufferAttr_HipcPointer> path, s32 option,
                      s64 size);
    Result DeleteFile(const InLargeData<FileSys::Sf::Path, BufferAttr_HipcPointer> path);
    Result CreateDirectory(const InLargeData<FileSys::Sf::Path, BufferAttr_HipcPointer> path);
    Result DeleteDirectory(const InLargeData<FileSys::Sf::Path, BufferAttr_HipcPointer> path);
    Result DeleteDirectoryRecursively(
        const InLargeData<FileSys::Sf::Path, BufferAttr_HipcPointer> path);
    Result CleanDirectoryRecursively(
        const InLargeData<FileSys::Sf::Path, BufferAttr_HipcPointer> path);
    Result RenameFile(const InLargeData<FileSys::Sf::Path, BufferAttr_HipcPointer> old_path,
                      const InLargeData<FileSys::Sf::Path, BufferAttr_HipcPointer> new_path);
    Result OpenFile(OutInterface<IFile> out_interface,
                    const InLargeData<FileSys::Sf::Path, BufferAttr_HipcPointer> path, u32 mode);
    Result OpenDirectory(OutInterface<IDirectory> out_interface,
                         const InLargeData<FileSys::Sf::Path, BufferAttr_HipcPointer> path,
                         u32 mode);
    Result GetEntryType(Out<u32> out_type,
                        const InLargeData<FileSys::Sf::Path, BufferAttr_HipcPointer> path);
    Result Commit();
    Result GetFreeSpaceSize(Out<s64> out_size,
                            const InLargeData<FileSys::Sf::Path, BufferAttr_HipcPointer> path);
    Result GetTotalSpaceSize(Out<s64> out_size,
                             const InLargeData<FileSys::Sf::Path, BufferAttr_HipcPointer> path);
    Result GetFileTimeStampRaw(Out<FileSys::FileTimeStampRaw> out_timestamp,
                               const InLargeData<FileSys::Sf::Path, BufferAttr_HipcPointer> path);
    Result GetFileSystemAttribute(Out<FileSystemAttribute> out_attribute);

private:
    std::unique_ptr<FileSys::Fsa::IFileSystem> backend;
    SizeGetter size_getter;
};

} // namespace Service::FileSystem
