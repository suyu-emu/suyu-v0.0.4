// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/file_sys/errors.h"
#include "core/hle/service/filesystem/fsp/fs_i_storage.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::FileSystem {

IStorage::IStorage(Core::System& system_, FileSys::VirtualFile backend_)
    : ServiceFramework{system_, "IStorage"}, backend(std::move(backend_)) {
    static const FunctionInfo functions[] = {
        {0, &IStorage::Read, "Read"},
        {1, nullptr, "Write"},
        {2, nullptr, "Flush"},
        {3, nullptr, "SetSize"},
        {4, &IStorage::GetSize, "GetSize"},
        {5, nullptr, "OperateRange"},
    };
    RegisterHandlers(functions);
}

void IStorage::Read(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const s64 offset = rp.Pop<s64>();
    const s64 length = rp.Pop<s64>();

    LOG_DEBUG(Service_FS, "called, offset=0x{:X}, length={}", offset, length);

    // Error checking
    if (length < 0) {
        LOG_ERROR(Service_FS, "Length is less than 0, length={}", length);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(FileSys::ERROR_INVALID_SIZE);
        return;
    }
    if (offset < 0) {
        LOG_ERROR(Service_FS, "Offset is less than 0, offset={}", offset);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(FileSys::ERROR_INVALID_OFFSET);
        return;
    }

    // Read the data from the Storage backend
    std::vector<u8> output = backend->ReadBytes(length, offset);
    // Write the data to memory
    ctx.WriteBuffer(output);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IStorage::GetSize(HLERequestContext& ctx) {
    const u64 size = backend->GetSize();
    LOG_DEBUG(Service_FS, "called, size={}", size);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push<u64>(size);
}

} // namespace Service::FileSystem
