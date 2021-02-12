// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/server_port.h"
#include "core/hle/kernel/session.h"
#include "core/hle/kernel/svc_results.h"

namespace Kernel {

ClientPort::ClientPort(KernelCore& kernel) : Object{kernel} {}
ClientPort::~ClientPort() = default;

std::shared_ptr<ServerPort> ClientPort::GetServerPort() const {
    return server_port;
}

ResultVal<std::shared_ptr<ClientSession>> ClientPort::Connect() {
    if (active_sessions >= max_sessions) {
        return ResultMaxConnectionsReached;
    }
    active_sessions++;

    auto [client, server] = Kernel::Session::Create(kernel, name);

    if (server_port->HasHLEHandler()) {
        server_port->GetHLEHandler()->ClientConnected(std::move(server));
    } else {
        server_port->AppendPendingSession(std::move(server));
    }

    return MakeResult(std::move(client));
}

void ClientPort::ConnectionClosed() {
    if (active_sessions == 0) {
        return;
    }

    --active_sessions;
}

} // namespace Kernel
