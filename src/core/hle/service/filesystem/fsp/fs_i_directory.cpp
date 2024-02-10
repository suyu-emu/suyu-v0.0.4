// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/file_sys/fs_filesystem.h"
#include "core/file_sys/savedata_factory.h"
#include "core/hle/service/filesystem/fsp/fs_i_directory.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::FileSystem {

IDirectory::IDirectory(Core::System& system_, FileSys::VirtualDir directory_,
                       FileSys::OpenDirectoryMode mode)
    : ServiceFramework{system_, "IDirectory"},
      backend(std::make_unique<FileSys::Fsa::IDirectory>(directory_, mode)) {
    static const FunctionInfo functions[] = {
        {0, &IDirectory::Read, "Read"},
        {1, &IDirectory::GetEntryCount, "GetEntryCount"},
    };
    RegisterHandlers(functions);
}

void IDirectory::Read(HLERequestContext& ctx) {
    LOG_DEBUG(Service_FS, "called.");

    // Calculate how many entries we can fit in the output buffer
    const u64 count_entries = ctx.GetWriteBufferNumElements<FileSys::DirectoryEntry>();

    s64 out_count{};
    FileSys::DirectoryEntry* out_entries = nullptr;
    const auto result = backend->Read(&out_count, out_entries, count_entries);

    // Write the data to memory
    ctx.WriteBuffer(out_entries, out_count);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(result);
    rb.Push(out_count);
}

void IDirectory::GetEntryCount(HLERequestContext& ctx) {
    LOG_DEBUG(Service_FS, "called");

    s64 out_count{};

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(backend->GetEntryCount(&out_count));
    rb.Push(out_count);
}

} // namespace Service::FileSystem
