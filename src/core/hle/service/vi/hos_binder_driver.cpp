// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/nvnflinger/binder.h"
#include "core/hle/service/nvnflinger/hos_binder_driver_server.h"
#include "core/hle/service/vi/hos_binder_driver.h"

namespace Service::VI {

IHOSBinderDriver::IHOSBinderDriver(Core::System& system_,
                                   Nvnflinger::HosBinderDriverServer& server_)
    : ServiceFramework{system_, "IHOSBinderDriver"}, server(server_) {
    static const FunctionInfo functions[] = {
        {0, &IHOSBinderDriver::TransactParcel, "TransactParcel"},
        {1, &IHOSBinderDriver::AdjustRefcount, "AdjustRefcount"},
        {2, &IHOSBinderDriver::GetNativeHandle, "GetNativeHandle"},
        {3, &IHOSBinderDriver::TransactParcel, "TransactParcelAuto"},
    };
    RegisterHandlers(functions);
}

IHOSBinderDriver::~IHOSBinderDriver() = default;

void IHOSBinderDriver::TransactParcel(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const u32 id = rp.Pop<u32>();
    const auto transaction = static_cast<android::TransactionId>(rp.Pop<u32>());
    const u32 flags = rp.Pop<u32>();

    LOG_DEBUG(Service_VI, "called. id=0x{:08X} transaction={:X}, flags=0x{:08X}", id, transaction,
              flags);

    server.TryGetProducer(id)->Transact(ctx, transaction, flags);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHOSBinderDriver::AdjustRefcount(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const u32 id = rp.Pop<u32>();
    const s32 addval = rp.PopRaw<s32>();
    const u32 type = rp.Pop<u32>();

    LOG_WARNING(Service_VI, "(STUBBED) called id={}, addval={:08X}, type={:08X}", id, addval, type);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHOSBinderDriver::GetNativeHandle(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const u32 id = rp.Pop<u32>();
    const u32 unknown = rp.Pop<u32>();

    LOG_WARNING(Service_VI, "(STUBBED) called id={}, unknown={:08X}", id, unknown);

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(server.TryGetProducer(id)->GetNativeHandle());
}

} // namespace Service::VI
