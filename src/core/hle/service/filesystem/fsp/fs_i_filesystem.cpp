// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/string_util.h"
#include "core/hle/service/filesystem/fsp/fs_i_directory.h"
#include "core/hle/service/filesystem/fsp/fs_i_file.h"
#include "core/hle/service/filesystem/fsp/fs_i_filesystem.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::FileSystem {

IFileSystem::IFileSystem(Core::System& system_, FileSys::VirtualDir backend_, SizeGetter size_)
    : ServiceFramework{system_, "IFileSystem"}, backend{std::move(backend_)}, size{std::move(
                                                                                  size_)} {
    static const FunctionInfo functions[] = {
        {0, &IFileSystem::CreateFile, "CreateFile"},
        {1, &IFileSystem::DeleteFile, "DeleteFile"},
        {2, &IFileSystem::CreateDirectory, "CreateDirectory"},
        {3, &IFileSystem::DeleteDirectory, "DeleteDirectory"},
        {4, &IFileSystem::DeleteDirectoryRecursively, "DeleteDirectoryRecursively"},
        {5, &IFileSystem::RenameFile, "RenameFile"},
        {6, nullptr, "RenameDirectory"},
        {7, &IFileSystem::GetEntryType, "GetEntryType"},
        {8, &IFileSystem::OpenFile, "OpenFile"},
        {9, &IFileSystem::OpenDirectory, "OpenDirectory"},
        {10, &IFileSystem::Commit, "Commit"},
        {11, &IFileSystem::GetFreeSpaceSize, "GetFreeSpaceSize"},
        {12, &IFileSystem::GetTotalSpaceSize, "GetTotalSpaceSize"},
        {13, &IFileSystem::CleanDirectoryRecursively, "CleanDirectoryRecursively"},
        {14, &IFileSystem::GetFileTimeStampRaw, "GetFileTimeStampRaw"},
        {15, nullptr, "QueryEntry"},
        {16, &IFileSystem::GetFileSystemAttribute, "GetFileSystemAttribute"},
    };
    RegisterHandlers(functions);
}

void IFileSystem::CreateFile(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    const auto file_buffer = ctx.ReadBuffer();
    const std::string name = Common::StringFromBuffer(file_buffer);

    const u64 file_mode = rp.Pop<u64>();
    const u32 file_size = rp.Pop<u32>();

    LOG_DEBUG(Service_FS, "called. file={}, mode=0x{:X}, size=0x{:08X}", name, file_mode,
              file_size);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(backend.CreateFile(name, file_size));
}

void IFileSystem::DeleteFile(HLERequestContext& ctx) {
    const auto file_buffer = ctx.ReadBuffer();
    const std::string name = Common::StringFromBuffer(file_buffer);

    LOG_DEBUG(Service_FS, "called. file={}", name);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(backend.DeleteFile(name));
}

void IFileSystem::CreateDirectory(HLERequestContext& ctx) {
    const auto file_buffer = ctx.ReadBuffer();
    const std::string name = Common::StringFromBuffer(file_buffer);

    LOG_DEBUG(Service_FS, "called. directory={}", name);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(backend.CreateDirectory(name));
}

void IFileSystem::DeleteDirectory(HLERequestContext& ctx) {
    const auto file_buffer = ctx.ReadBuffer();
    const std::string name = Common::StringFromBuffer(file_buffer);

    LOG_DEBUG(Service_FS, "called. directory={}", name);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(backend.DeleteDirectory(name));
}

void IFileSystem::DeleteDirectoryRecursively(HLERequestContext& ctx) {
    const auto file_buffer = ctx.ReadBuffer();
    const std::string name = Common::StringFromBuffer(file_buffer);

    LOG_DEBUG(Service_FS, "called. directory={}", name);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(backend.DeleteDirectoryRecursively(name));
}

void IFileSystem::CleanDirectoryRecursively(HLERequestContext& ctx) {
    const auto file_buffer = ctx.ReadBuffer();
    const std::string name = Common::StringFromBuffer(file_buffer);

    LOG_DEBUG(Service_FS, "called. Directory: {}", name);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(backend.CleanDirectoryRecursively(name));
}

void IFileSystem::RenameFile(HLERequestContext& ctx) {
    const std::string src_name = Common::StringFromBuffer(ctx.ReadBuffer(0));
    const std::string dst_name = Common::StringFromBuffer(ctx.ReadBuffer(1));

    LOG_DEBUG(Service_FS, "called. file '{}' to file '{}'", src_name, dst_name);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(backend.RenameFile(src_name, dst_name));
}

void IFileSystem::OpenFile(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    const auto file_buffer = ctx.ReadBuffer();
    const std::string name = Common::StringFromBuffer(file_buffer);

    const auto mode = static_cast<FileSys::OpenMode>(rp.Pop<u32>());

    LOG_DEBUG(Service_FS, "called. file={}, mode={}", name, mode);

    FileSys::VirtualFile vfs_file{};
    auto result = backend.OpenFile(&vfs_file, name, mode);
    if (result != ResultSuccess) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
        return;
    }

    auto file = std::make_shared<IFile>(system, vfs_file);

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IFile>(std::move(file));
}

void IFileSystem::OpenDirectory(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    const auto file_buffer = ctx.ReadBuffer();
    const std::string name = Common::StringFromBuffer(file_buffer);
    const auto mode = rp.PopRaw<FileSys::OpenDirectoryMode>();

    LOG_DEBUG(Service_FS, "called. directory={}, mode={}", name, mode);

    FileSys::VirtualDir vfs_dir{};
    auto result = backend.OpenDirectory(&vfs_dir, name);
    if (result != ResultSuccess) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
        return;
    }

    auto directory = std::make_shared<IDirectory>(system, vfs_dir, mode);

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IDirectory>(std::move(directory));
}

void IFileSystem::GetEntryType(HLERequestContext& ctx) {
    const auto file_buffer = ctx.ReadBuffer();
    const std::string name = Common::StringFromBuffer(file_buffer);

    LOG_DEBUG(Service_FS, "called. file={}", name);

    FileSys::DirectoryEntryType vfs_entry_type{};
    auto result = backend.GetEntryType(&vfs_entry_type, name);
    if (result != ResultSuccess) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u32>(static_cast<u32>(vfs_entry_type));
}

void IFileSystem::Commit(HLERequestContext& ctx) {
    LOG_WARNING(Service_FS, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IFileSystem::GetFreeSpaceSize(HLERequestContext& ctx) {
    LOG_DEBUG(Service_FS, "called");

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push(size.get_free_size());
}

void IFileSystem::GetTotalSpaceSize(HLERequestContext& ctx) {
    LOG_DEBUG(Service_FS, "called");

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push(size.get_total_size());
}

void IFileSystem::GetFileTimeStampRaw(HLERequestContext& ctx) {
    const auto file_buffer = ctx.ReadBuffer();
    const std::string name = Common::StringFromBuffer(file_buffer);

    LOG_WARNING(Service_FS, "(Partial Implementation) called. file={}", name);

    FileSys::FileTimeStampRaw vfs_timestamp{};
    auto result = backend.GetFileTimeStampRaw(&vfs_timestamp, name);
    if (result != ResultSuccess) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 10};
    rb.Push(ResultSuccess);
    rb.PushRaw(vfs_timestamp);
}

void IFileSystem::GetFileSystemAttribute(HLERequestContext& ctx) {
    LOG_WARNING(Service_FS, "(STUBBED) called");

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
    static_assert(sizeof(FileSystemAttribute) == 0xc0, "FileSystemAttribute has incorrect size");

    FileSystemAttribute savedata_attribute{};
    savedata_attribute.dir_entry_name_length_max_defined = true;
    savedata_attribute.file_entry_name_length_max_defined = true;
    savedata_attribute.dir_entry_name_length_max = 0x40;
    savedata_attribute.file_entry_name_length_max = 0x40;

    IPC::ResponseBuilder rb{ctx, 50};
    rb.Push(ResultSuccess);
    rb.PushRaw(savedata_attribute);
}

} // namespace Service::FileSystem
