// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/kernel/k_client_port.h"
#include "core/hle/kernel/k_server_port.h"
#include "core/hle/kernel/k_session.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/svc_results.h"

namespace Kernel {

KClientPort::KClientPort(KernelCore& kernel) : KSynchronizationObject{kernel} {}
KClientPort::~KClientPort() = default;

void KClientPort::Initialize(s32 max_sessions_, std::string&& name_) {
    max_sessions = max_sessions_;
    name = std::move(name_);
}

KServerPort* KClientPort::GetServerPort() const {
    return server_port;
}

ResultVal<KClientSession*> KClientPort::Connect() {
    if (num_sessions >= max_sessions) {
        return ResultOutOfSessions;
    }
    num_sessions++;

    auto* session = Kernel::KSession::Create(kernel);
    session->Initialize(name + ":ClientPort");

    if (server_port->HasHLEHandler()) {
        server_port->GetHLEHandler()->ClientConnected(session);
    } else {
        server_port->AppendPendingSession(std::addressof(session->GetServerSession()));
    }

    return MakeResult(std::addressof(session->GetClientSession()));
}

void KClientPort::ConnectionClosed() {
    if (num_sessions == 0) {
        return;
    }

    --num_sessions;
}

void KClientPort::Destroy() {}

bool KClientPort::IsSignaled() const {
    return num_sessions < max_sessions;
}

} // namespace Kernel
