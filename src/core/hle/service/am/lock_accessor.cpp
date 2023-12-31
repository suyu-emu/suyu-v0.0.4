// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/am/lock_accessor.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::AM {

ILockAccessor::ILockAccessor(Core::System& system_)
    : ServiceFramework{system_, "ILockAccessor"}, service_context{system_, "ILockAccessor"} {
    // clang-format off
        static const FunctionInfo functions[] = {
            {1, &ILockAccessor::TryLock, "TryLock"},
            {2, &ILockAccessor::Unlock, "Unlock"},
            {3, &ILockAccessor::GetEvent, "GetEvent"},
            {4,&ILockAccessor::IsLocked, "IsLocked"},
        };
    // clang-format on

    RegisterHandlers(functions);

    lock_event = service_context.CreateEvent("ILockAccessor::LockEvent");
}

ILockAccessor::~ILockAccessor() {
    service_context.CloseEvent(lock_event);
};

void ILockAccessor::TryLock(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto return_handle = rp.Pop<bool>();

    LOG_WARNING(Service_AM, "(STUBBED) called, return_handle={}", return_handle);

    // TODO: When return_handle is true this function should return the lock handle

    is_locked = true;

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u8>(is_locked);
}

void ILockAccessor::Unlock(HLERequestContext& ctx) {
    LOG_INFO(Service_AM, "called");

    is_locked = false;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ILockAccessor::GetEvent(HLERequestContext& ctx) {
    LOG_INFO(Service_AM, "called");

    lock_event->Signal();

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(lock_event->GetReadableEvent());
}

void ILockAccessor::IsLocked(HLERequestContext& ctx) {
    LOG_INFO(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
    rb.Push<u8>(is_locked);
}

} // namespace Service::AM
