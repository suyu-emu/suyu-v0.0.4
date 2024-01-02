// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/am/am_results.h"
#include "core/hle/service/am/library_applet_storage.h"
#include "core/hle/service/am/storage.h"
#include "core/hle/service/am/storage_accessor.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::AM {

IStorage::IStorage(Core::System& system_, std::shared_ptr<LibraryAppletStorage> impl_)
    : ServiceFramework{system_, "IStorage"}, impl{std::move(impl_)} {
    static const FunctionInfo functions[] = {
        {0, &IStorage::Open, "Open"},
        {1, &IStorage::OpenTransferStorage, "OpenTransferStorage"},
    };

    RegisterHandlers(functions);
}

IStorage::IStorage(Core::System& system_, std::vector<u8>&& data)
    : IStorage(system_, CreateStorage(std::move(data))) {}

IStorage::~IStorage() = default;

void IStorage::Open(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    if (impl->GetHandle() != nullptr) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(AM::ResultInvalidStorageType);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IStorageAccessor>(system, impl);
}

void IStorage::OpenTransferStorage(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    if (impl->GetHandle() == nullptr) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(AM::ResultInvalidStorageType);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<ITransferStorageAccessor>(system, impl);
}

std::vector<u8> IStorage::GetData() const {
    return impl->GetData();
}

} // namespace Service::AM
