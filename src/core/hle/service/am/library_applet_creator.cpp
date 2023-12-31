// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/kernel/k_transfer_memory.h"
#include "core/hle/service/am/frontend/applets.h"
#include "core/hle/service/am/library_applet_accessor.h"
#include "core/hle/service/am/library_applet_creator.h"
#include "core/hle/service/am/storage.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::AM {

ILibraryAppletCreator::ILibraryAppletCreator(Core::System& system_)
    : ServiceFramework{system_, "ILibraryAppletCreator"} {
    static const FunctionInfo functions[] = {
        {0, &ILibraryAppletCreator::CreateLibraryApplet, "CreateLibraryApplet"},
        {1, nullptr, "TerminateAllLibraryApplets"},
        {2, nullptr, "AreAnyLibraryAppletsLeft"},
        {10, &ILibraryAppletCreator::CreateStorage, "CreateStorage"},
        {11, &ILibraryAppletCreator::CreateTransferMemoryStorage, "CreateTransferMemoryStorage"},
        {12, &ILibraryAppletCreator::CreateHandleStorage, "CreateHandleStorage"},
    };
    RegisterHandlers(functions);
}

ILibraryAppletCreator::~ILibraryAppletCreator() = default;

void ILibraryAppletCreator::CreateLibraryApplet(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    const auto applet_id = rp.PopRaw<AppletId>();
    const auto applet_mode = rp.PopRaw<LibraryAppletMode>();

    LOG_DEBUG(Service_AM, "called with applet_id={:08X}, applet_mode={:08X}", applet_id,
              applet_mode);

    const auto& holder{system.GetFrontendAppletHolder()};
    const auto applet = holder.GetApplet(applet_id, applet_mode);

    if (applet == nullptr) {
        LOG_ERROR(Service_AM, "Applet doesn't exist! applet_id={}", applet_id);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultUnknown);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};

    rb.Push(ResultSuccess);
    rb.PushIpcInterface<ILibraryAppletAccessor>(system, applet);
}

void ILibraryAppletCreator::CreateStorage(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    const s64 size{rp.Pop<s64>()};

    LOG_DEBUG(Service_AM, "called, size={}", size);

    if (size <= 0) {
        LOG_ERROR(Service_AM, "size is less than or equal to 0");
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultUnknown);
        return;
    }

    std::vector<u8> buffer(size);

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IStorage>(system, std::move(buffer));
}

void ILibraryAppletCreator::CreateTransferMemoryStorage(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    struct Parameters {
        u8 permissions;
        s64 size;
    };

    const auto parameters{rp.PopRaw<Parameters>()};
    const auto handle{ctx.GetCopyHandle(0)};

    LOG_DEBUG(Service_AM, "called, permissions={}, size={}, handle={:08X}", parameters.permissions,
              parameters.size, handle);

    if (parameters.size <= 0) {
        LOG_ERROR(Service_AM, "size is less than or equal to 0");
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultUnknown);
        return;
    }

    auto transfer_mem = ctx.GetObjectFromHandle<Kernel::KTransferMemory>(handle);

    if (transfer_mem.IsNull()) {
        LOG_ERROR(Service_AM, "transfer_mem is a nullptr for handle={:08X}", handle);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultUnknown);
        return;
    }

    std::vector<u8> memory(transfer_mem->GetSize());
    ctx.GetMemory().ReadBlock(transfer_mem->GetSourceAddress(), memory.data(), memory.size());

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IStorage>(system, std::move(memory));
}

void ILibraryAppletCreator::CreateHandleStorage(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    const s64 size{rp.Pop<s64>()};
    const auto handle{ctx.GetCopyHandle(0)};

    LOG_DEBUG(Service_AM, "called, size={}, handle={:08X}", size, handle);

    if (size <= 0) {
        LOG_ERROR(Service_AM, "size is less than or equal to 0");
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultUnknown);
        return;
    }

    auto transfer_mem = ctx.GetObjectFromHandle<Kernel::KTransferMemory>(handle);

    if (transfer_mem.IsNull()) {
        LOG_ERROR(Service_AM, "transfer_mem is a nullptr for handle={:08X}", handle);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultUnknown);
        return;
    }

    std::vector<u8> memory(transfer_mem->GetSize());
    ctx.GetMemory().ReadBlock(transfer_mem->GetSourceAddress(), memory.data(), memory.size());

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IStorage>(system, std::move(memory));
}

} // namespace Service::AM
