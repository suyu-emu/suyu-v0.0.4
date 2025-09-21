// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/kernel/k_client_port.h"
#include "core/hle/kernel/k_port.h"
#include "core/hle/kernel/k_scoped_resource_reservation.h"
#include "core/hle/kernel/k_server_session.h"
#include "core/hle/kernel/k_session.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/sm/sm_controller.h"

namespace Service::SM {

void Controller::ConvertCurrentObjectToDomain(HLERequestContext& ctx) {
    ASSERT_MSG(!ctx.GetManager()->IsDomain(), "Session is already a domain");
    LOG_DEBUG(Service, "called, server_session={}", ctx.Session()->GetId());
    ctx.GetManager()->ConvertToDomainOnRequestEnd();

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u32>(1); // Converted sessions start with 1 request handler
}

void Controller::CloneCurrentObject(HLERequestContext& ctx) {
    LOG_DEBUG(Service, "called");

    auto session_manager = ctx.GetManager();

    // FIXME: this is duplicated from the SVC, it should just call it instead
    // once this is a proper process

    // Reserve a new session from the process resource limit.
    Kernel::KScopedResourceReservation session_reservation(
        Kernel::GetCurrentProcessPointer(kernel), Kernel::LimitableResource::SessionCountMax);
    ASSERT(session_reservation.Succeeded());

    // Create the session.
    Kernel::KSession* session = Kernel::KSession::Create(kernel);
    ASSERT(session != nullptr);

    // Initialize the session.
    session->Initialize(nullptr, 0);

    // Commit the session reservation.
    session_reservation.Commit();

    // Register the session.
    Kernel::KSession::Register(kernel, session);

    // Register with server manager.
    session_manager->GetServerManager().RegisterSession(&session->GetServerSession(),
                                                        session_manager);

    // We succeeded.
    IPC::ResponseBuilder rb{ctx, 2, 0, 1, IPC::ResponseBuilder::Flags::AlwaysMoveHandles};
    rb.Push(ResultSuccess);
    rb.PushMoveObjects(session->GetClientSession());
}

void Controller::CloneCurrentObjectEx(HLERequestContext& ctx) {
    LOG_DEBUG(Service, "called");

    CloneCurrentObject(ctx);
}

void Controller::QueryPointerBufferSize(HLERequestContext& ctx) {
    LOG_DEBUG(Service, "called");

    auto* process = Kernel::GetCurrentProcessPointer(kernel);
    ASSERT(process != nullptr);

    u32 buffer_size = process->GetPointerBufferSize();
    if (buffer_size > (std::numeric_limits<u16>::max)()) {
        LOG_WARNING(Service, "Pointer buffer size exceeds u16 max, clamping");
        buffer_size = (std::numeric_limits<u16>::max)();
    }

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u16>(static_cast<u16>(buffer_size));
}

void Controller::SetPointerBufferSize(HLERequestContext& ctx) {
    LOG_DEBUG(Service, "called");

    auto* process = Kernel::GetCurrentProcessPointer(kernel);
    ASSERT(process != nullptr);

    IPC::RequestParser rp{ctx};

    u32 requested_size = rp.PopRaw<u32>();

    if (requested_size > (std::numeric_limits<u16>::max)()) {
        LOG_WARNING(Service, "Requested pointer buffer size too large, clamping to 0xFFFF");
        requested_size = (std::numeric_limits<u16>::max)();
    }

    process->SetPointerBufferSize(requested_size);

    LOG_INFO(Service, "Pointer buffer size dynamically updated to {:#x} bytes by process", requested_size);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}


// https://switchbrew.org/wiki/IPC_Marshalling
Controller::Controller(Core::System& system_) : ServiceFramework{system_, "IpcController"} {
    static const FunctionInfo functions[] = {
        {0, &Controller::ConvertCurrentObjectToDomain, "ConvertCurrentObjectToDomain"},
        {1, nullptr, "CopyFromCurrentDomain"},
        {2, &Controller::CloneCurrentObject, "CloneCurrentObject"},
        {3, &Controller::QueryPointerBufferSize, "QueryPointerBufferSize"},
        {4, &Controller::CloneCurrentObjectEx, "CloneCurrentObjectEx"},
        {5, &Controller::SetPointerBufferSize, "SetPointerBufferSize"},
    };
    RegisterHandlers(functions);
}

Controller::~Controller() = default;

} // namespace Service::SM
