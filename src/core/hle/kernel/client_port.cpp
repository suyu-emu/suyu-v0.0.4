// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/kernel/k_session.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/server_port.h"
#include "core/hle/kernel/svc_results.h"

namespace Kernel {

ClientPort::ClientPort(KernelCore& kernel) : Object{kernel} {}
ClientPort::~ClientPort() = default;

std::shared_ptr<ServerPort> ClientPort::GetServerPort() const {
    return server_port;
}

ResultVal<KClientSession*> ClientPort::Connect() {
    if (active_sessions >= max_sessions) {
        return ResultOutOfSessions;
    }
    active_sessions++;

    auto* session = Kernel::KSession::Create(kernel);
    session->Initialize(name + ":ClientPort");

    if (server_port->HasHLEHandler()) {
        server_port->GetHLEHandler()->ClientConnected(session);
    } else {
        server_port->AppendPendingSession(std::addressof(session->GetServerSession()));
    }

    return MakeResult(std::addressof(session->GetClientSession()));
}

void ClientPort::ConnectionClosed() {
    if (active_sessions == 0) {
        return;
    }

    --active_sessions;
}

} // namespace Kernel
