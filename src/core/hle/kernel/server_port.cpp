// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <tuple>
#include "common/assert.h"
#include "core/hle/kernel/k_client_port.h"
#include "core/hle/kernel/k_server_session.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/server_port.h"
#include "core/hle/kernel/svc_results.h"

namespace Kernel {

ServerPort::ServerPort(KernelCore& kernel) : KSynchronizationObject{kernel} {}
ServerPort::~ServerPort() = default;

ResultVal<KServerSession*> ServerPort::Accept() {
    if (pending_sessions.empty()) {
        return ResultNotFound;
    }

    auto* session = pending_sessions.back();
    pending_sessions.pop_back();
    return MakeResult(session);
}

void ServerPort::AppendPendingSession(KServerSession* pending_session) {
    pending_sessions.push_back(std::move(pending_session));
    if (pending_sessions.size() == 1) {
        NotifyAvailable();
    }
}

bool ServerPort::IsSignaled() const {
    return !pending_sessions.empty();
}

ServerPort::PortPair ServerPort::CreatePortPair(KernelCore& kernel, u32 max_sessions,
                                                std::string name) {
    std::shared_ptr<ServerPort> server_port = std::make_shared<ServerPort>(kernel);
    KClientPort* client_port = new KClientPort(kernel);

    KAutoObject::Create(client_port);

    client_port->Initialize(max_sessions, name + "_Client");
    client_port->server_port = server_port;

    server_port->name = name + "_Server";

    return std::make_pair(std::move(server_port), client_port);
}

} // namespace Kernel
