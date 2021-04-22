// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <tuple>
#include "common/assert.h"
#include "core/hle/kernel/k_client_port.h"
#include "core/hle/kernel/k_server_port.h"
#include "core/hle/kernel/k_server_session.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/svc_results.h"

namespace Kernel {

KServerPort::KServerPort(KernelCore& kernel) : KSynchronizationObject{kernel} {}
KServerPort::~KServerPort() = default;

void KServerPort::Initialize(std::string&& name_) {
    // Set member variables.
    name = std::move(name_);
}

ResultVal<KServerSession*> KServerPort::Accept() {
    if (pending_sessions.empty()) {
        return ResultNotFound;
    }

    auto* session = pending_sessions.back();
    pending_sessions.pop_back();
    return MakeResult(session);
}

void KServerPort::AppendPendingSession(KServerSession* pending_session) {
    pending_sessions.push_back(std::move(pending_session));
    if (pending_sessions.size() == 1) {
        NotifyAvailable();
    }
}

void KServerPort::Destroy() {}

bool KServerPort::IsSignaled() const {
    return !pending_sessions.empty();
}

KServerPort::PortPair KServerPort::CreatePortPair(KernelCore& kernel, u32 max_sessions,
                                                  std::string name) {
    KServerPort* server_port = new KServerPort(kernel);
    KClientPort* client_port = new KClientPort(kernel);

    KAutoObject::Create(server_port);
    KAutoObject::Create(client_port);

    server_port->Initialize(name + "_Server");
    client_port->Initialize(max_sessions, name + "_Client");

    client_port->server_port = server_port;

    server_port->name = name + "_Server";

    return std::make_pair(server_port, client_port);
}

} // namespace Kernel
