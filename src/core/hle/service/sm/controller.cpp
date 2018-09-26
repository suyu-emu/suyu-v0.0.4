// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/kernel/session.h"
#include "core/hle/service/sm/controller.h"

namespace Service::SM {

void Controller::ConvertSessionToDomain(Kernel::HLERequestContext& ctx) {
    ASSERT_MSG(ctx.Session()->IsSession(), "Session is already a domain");
    ctx.Session()->ConvertToDomain();

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(1); // Converted sessions start with 1 request handler

    LOG_DEBUG(Service, "called, server_session={}", ctx.Session()->GetObjectId());
}

void Controller::DuplicateSession(Kernel::HLERequestContext& ctx) {
    // TODO(bunnei): This is just creating a new handle to the same Session. I assume this is wrong
    // and that we probably want to actually make an entirely new Session, but we still need to
    // verify this on hardware.
    IPC::ResponseBuilder rb{ctx, 2, 0, 1, IPC::ResponseBuilder::Flags::AlwaysMoveHandles};
    rb.Push(RESULT_SUCCESS);
    Kernel::SharedPtr<Kernel::ClientSession> session{ctx.Session()->parent->client};
    rb.PushMoveObjects(session);

    LOG_DEBUG(Service, "called, session={}", session->GetObjectId());
}

void Controller::DuplicateSessionEx(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service, "(STUBBED) called, using DuplicateSession");

    DuplicateSession(ctx);
}

void Controller::QueryPointerBufferSize(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(RESULT_SUCCESS);
    rb.Push<u16>(0x500);

    LOG_WARNING(Service, "(STUBBED) called");
}

Controller::Controller() : ServiceFramework("IpcController") {
    static const FunctionInfo functions[] = {
        {0x00000000, &Controller::ConvertSessionToDomain, "ConvertSessionToDomain"},
        {0x00000001, nullptr, "ConvertDomainToSession"},
        {0x00000002, &Controller::DuplicateSession, "DuplicateSession"},
        {0x00000003, &Controller::QueryPointerBufferSize, "QueryPointerBufferSize"},
        {0x00000004, &Controller::DuplicateSessionEx, "DuplicateSessionEx"},
    };
    RegisterHandlers(functions);
}

Controller::~Controller() = default;

} // namespace Service::SM
