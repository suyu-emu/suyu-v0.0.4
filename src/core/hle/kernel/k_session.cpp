// Copyright 2021 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "core/hle/kernel/k_client_port.h"
#include "core/hle/kernel/k_client_session.h"
#include "core/hle/kernel/k_scoped_resource_reservation.h"
#include "core/hle/kernel/k_server_session.h"
#include "core/hle/kernel/k_session.h"

namespace Kernel {

KSession::KSession(KernelCore& kernel)
    : KAutoObjectWithSlabHeapAndContainer{kernel}, server{kernel}, client{kernel} {}
KSession::~KSession() = default;

void KSession::Initialize(KClientPort* port_, const std::string& name_) {
    // Increment reference count.
    // Because reference count is one on creation, this will result
    // in a reference count of two. Thus, when both server and client are closed
    // this object will be destroyed.
    Open();

    // Create our sub sessions.
    KAutoObject::Create(std::addressof(server));
    KAutoObject::Create(std::addressof(client));

    // Initialize our sub sessions.
    server.Initialize(this, name_ + ":Server");
    client.Initialize(this, name_ + ":Client");

    // Set state and name.
    SetState(State::Normal);
    name = name_;

    // Set our owner process.
    process = kernel.CurrentProcess();
    process->Open();

    // Set our port.
    port = port_;
    if (port != nullptr) {
        port->Open();
    }

    // Mark initialized.
    initialized = true;
}

void KSession::Finalize() {
    if (port != nullptr) {
        port->OnSessionFinalized();
        port->Close();
    }
}

void KSession::OnServerClosed() {
    if (GetState() == State::Normal) {
        SetState(State::ServerClosed);
        client.OnServerClosed();
    }
}

void KSession::OnClientClosed() {
    if (GetState() == State::Normal) {
        SetState(State::ClientClosed);
        server.OnClientClosed();
    }
}

void KSession::PostDestroy(uintptr_t arg) {
    // Release the session count resource the owner process holds.
    Process* owner = reinterpret_cast<Process*>(arg);
    owner->GetResourceLimit()->Release(LimitableResource::Sessions, 1);
    owner->Close();
}

} // namespace Kernel
