// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/k_client_port.h"
#include "core/hle/kernel/k_client_session.h"
#include "core/hle/kernel/k_port.h"
#include "core/hle/kernel/k_scoped_resource_reservation.h"
#include "core/hle/kernel/k_server_port.h"
#include "core/hle/kernel/k_server_session.h"
#include "core/hle/kernel/k_session.h"
#include "core/hle/service/sm/controller.h"

namespace Service::SM {

void Controller::ConvertCurrentObjectToDomain(Kernel::HLERequestContext& ctx) {
    ASSERT_MSG(!ctx.Session()->IsDomain(), "Session is already a domain");
    LOG_DEBUG(Service, "called, server_session={}", ctx.Session()->GetId());
    ctx.Session()->ConvertToDomain();

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u32>(1); // Converted sessions start with 1 request handler
}

void Controller::CloneCurrentObject(Kernel::HLERequestContext& ctx) {
    // TODO(bunnei): This is just creating a new handle to the same Session. I assume this is wrong
    // and that we probably want to actually make an entirely new Session, but we still need to
    // verify this on hardware.

    LOG_DEBUG(Service, "called");

    auto& kernel = system.Kernel();
    auto* session = ctx.Session()->GetParent();
    auto* port = session->GetParent()->GetParent();

    // Reserve a new session from the process resource limit.
    Kernel::KScopedResourceReservation session_reservation(
        kernel.CurrentProcess()->GetResourceLimit(), Kernel::LimitableResource::Sessions);
    if (!session_reservation.Succeeded()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(Kernel::ResultLimitReached);
    }

    // Create a new session.
    auto* clone = Kernel::KSession::Create(kernel);
    clone->Initialize(&port->GetClientPort(), session->GetName());

    // Commit the session reservation.
    session_reservation.Commit();

    // Enqueue the session with the named port.
    port->EnqueueSession(&clone->GetServerSession());

    // Set the session request manager.
    clone->GetServerSession().SetSessionRequestManager(
        session->GetServerSession().GetSessionRequestManager());

    // We succeeded.
    IPC::ResponseBuilder rb{ctx, 2, 0, 1, IPC::ResponseBuilder::Flags::AlwaysMoveHandles};
    rb.Push(ResultSuccess);
    rb.PushMoveObjects(clone->GetClientSession());
}

void Controller::CloneCurrentObjectEx(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service, "called");

    CloneCurrentObject(ctx);
}

void Controller::QueryPointerBufferSize(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u16>(0x8000);
}

// https://switchbrew.org/wiki/IPC_Marshalling
Controller::Controller(Core::System& system_) : ServiceFramework{system_, "IpcController"} {
    static const FunctionInfo functions[] = {
        {0, &Controller::ConvertCurrentObjectToDomain, "ConvertCurrentObjectToDomain"},
        {1, nullptr, "CopyFromCurrentDomain"},
        {2, &Controller::CloneCurrentObject, "CloneCurrentObject"},
        {3, &Controller::QueryPointerBufferSize, "QueryPointerBufferSize"},
        {4, &Controller::CloneCurrentObjectEx, "CloneCurrentObjectEx"},
    };
    RegisterHandlers(functions);
}

Controller::~Controller() = default;

} // namespace Service::SM
