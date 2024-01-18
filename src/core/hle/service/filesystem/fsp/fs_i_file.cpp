// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/file_sys/errors.h"
#include "core/hle/service/filesystem/fsp/fs_i_file.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::FileSystem {

IFile::IFile(Core::System& system_, FileSys::VirtualFile backend_)
    : ServiceFramework{system_, "IFile"}, backend(std::move(backend_)) {
    static const FunctionInfo functions[] = {
        {0, &IFile::Read, "Read"},
        {1, &IFile::Write, "Write"},
        {2, &IFile::Flush, "Flush"},
        {3, &IFile::SetSize, "SetSize"},
        {4, &IFile::GetSize, "GetSize"},
        {5, nullptr, "OperateRange"},
        {6, nullptr, "OperateRangeWithBuffer"},
    };
    RegisterHandlers(functions);
}

void IFile::Read(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const u64 option = rp.Pop<u64>();
    const s64 offset = rp.Pop<s64>();
    const s64 length = rp.Pop<s64>();

    LOG_DEBUG(Service_FS, "called, option={}, offset=0x{:X}, length={}", option, offset, length);

    // Error checking
    if (length < 0) {
        LOG_ERROR(Service_FS, "Length is less than 0, length={}", length);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(FileSys::ResultInvalidSize);
        return;
    }
    if (offset < 0) {
        LOG_ERROR(Service_FS, "Offset is less than 0, offset={}", offset);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(FileSys::ResultInvalidOffset);
        return;
    }

    // Read the data from the Storage backend
    std::vector<u8> output = backend->ReadBytes(length, offset);

    // Write the data to memory
    ctx.WriteBuffer(output);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push(static_cast<u64>(output.size()));
}

void IFile::Write(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const u64 option = rp.Pop<u64>();
    const s64 offset = rp.Pop<s64>();
    const s64 length = rp.Pop<s64>();

    LOG_DEBUG(Service_FS, "called, option={}, offset=0x{:X}, length={}", option, offset, length);

    // Error checking
    if (length < 0) {
        LOG_ERROR(Service_FS, "Length is less than 0, length={}", length);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(FileSys::ResultInvalidSize);
        return;
    }
    if (offset < 0) {
        LOG_ERROR(Service_FS, "Offset is less than 0, offset={}", offset);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(FileSys::ResultInvalidOffset);
        return;
    }

    const auto data = ctx.ReadBuffer();

    ASSERT_MSG(static_cast<s64>(data.size()) <= length,
               "Attempting to write more data than requested (requested={:016X}, actual={:016X}).",
               length, data.size());

    // Write the data to the Storage backend
    const auto write_size =
        static_cast<std::size_t>(std::distance(data.begin(), data.begin() + length));
    const std::size_t written = backend->Write(data.data(), write_size, offset);

    ASSERT_MSG(static_cast<s64>(written) == length,
               "Could not write all bytes to file (requested={:016X}, actual={:016X}).", length,
               written);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IFile::Flush(HLERequestContext& ctx) {
    LOG_DEBUG(Service_FS, "called");

    // Exists for SDK compatibiltity -- No need to flush file.

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IFile::SetSize(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const u64 size = rp.Pop<u64>();
    LOG_DEBUG(Service_FS, "called, size={}", size);

    backend->Resize(size);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IFile::GetSize(HLERequestContext& ctx) {
    const u64 size = backend->GetSize();
    LOG_DEBUG(Service_FS, "called, size={}", size);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push<u64>(size);
}

} // namespace Service::FileSystem
