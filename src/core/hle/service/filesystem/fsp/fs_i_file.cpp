// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/file_sys/errors.h"
#include "core/hle/service/filesystem/fsp/fs_i_file.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::FileSystem {

IFile::IFile(Core::System& system_, FileSys::VirtualFile file_)
    : ServiceFramework{system_, "IFile"}, backend{std::make_unique<FileSys::Fsa::IFile>(file_)} {
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

    // Read the data from the Storage backend
    std::vector<u8> output(length);
    std::size_t bytes_read;
    const auto result = backend->Read(&bytes_read, offset, output.data(), length);

    // Write the data to memory
    ctx.WriteBuffer(output);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(result);
    rb.Push(static_cast<u64>(bytes_read));
}

void IFile::Write(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto option = rp.PopRaw<FileSys::WriteOption>();
    [[maybe_unused]] const u32 unused = rp.Pop<u32>();
    const s64 offset = rp.Pop<s64>();
    const s64 length = rp.Pop<s64>();

    LOG_DEBUG(Service_FS, "called, option={}, offset=0x{:X}, length={}", option.value, offset,
              length);

    const auto data = ctx.ReadBuffer();

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(backend->Write(offset, data.data(), length, option));
}

void IFile::Flush(HLERequestContext& ctx) {
    LOG_DEBUG(Service_FS, "called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(backend->Flush());
}

void IFile::SetSize(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const u64 size = rp.Pop<u64>();
    LOG_DEBUG(Service_FS, "called, size={}", size);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(backend->SetSize(size));
}

void IFile::GetSize(HLERequestContext& ctx) {
    s64 size;
    const auto result = backend->GetSize(&size);
    LOG_DEBUG(Service_FS, "called, size={}", size);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(result);
    rb.Push<u64>(size);
}

} // namespace Service::FileSystem
