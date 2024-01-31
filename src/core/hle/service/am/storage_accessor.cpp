// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/kernel/k_transfer_memory.h"
#include "core/hle/service/am/am_results.h"
#include "core/hle/service/am/library_applet_storage.h"
#include "core/hle/service/am/storage_accessor.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::AM {

IStorageAccessor::IStorageAccessor(Core::System& system_,
                                   std::shared_ptr<LibraryAppletStorage> impl_)
    : ServiceFramework{system_, "IStorageAccessor"}, impl{std::move(impl_)} {
    static const FunctionInfo functions[] = {
        {0, &IStorageAccessor::GetSize, "GetSize"},
        {10, &IStorageAccessor::Write, "Write"},
        {11, &IStorageAccessor::Read, "Read"},
    };

    RegisterHandlers(functions);
}

IStorageAccessor::~IStorageAccessor() = default;

void IStorageAccessor::GetSize(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 4};

    rb.Push(ResultSuccess);
    rb.Push(impl->GetSize());
}

void IStorageAccessor::Write(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    const s64 offset{rp.Pop<s64>()};
    const auto data{ctx.ReadBuffer()};
    LOG_DEBUG(Service_AM, "called, offset={}, size={}", offset, data.size());

    const auto res{impl->Write(offset, data.data(), data.size())};

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void IStorageAccessor::Read(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    const s64 offset{rp.Pop<s64>()};
    std::vector<u8> data(ctx.GetWriteBufferSize());

    LOG_DEBUG(Service_AM, "called, offset={}, size={}", offset, data.size());

    const auto res{impl->Read(offset, data.data(), data.size())};

    ctx.WriteBuffer(data);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

ITransferStorageAccessor::ITransferStorageAccessor(Core::System& system_,
                                                   std::shared_ptr<LibraryAppletStorage> impl_)
    : ServiceFramework{system_, "ITransferStorageAccessor"}, impl{std::move(impl_)} {
    static const FunctionInfo functions[] = {
        {0, &ITransferStorageAccessor::GetSize, "GetSize"},
        {1, &ITransferStorageAccessor::GetHandle, "GetHandle"},
    };

    RegisterHandlers(functions);
}

ITransferStorageAccessor::~ITransferStorageAccessor() = default;

void ITransferStorageAccessor::GetSize(HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push(impl->GetSize());
}

void ITransferStorageAccessor::GetHandle(HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 4, 1};
    rb.Push(ResultSuccess);
    rb.Push(impl->GetSize());
    rb.PushCopyObjects(impl->GetHandle());
}

} // namespace Service::AM
