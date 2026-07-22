// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/kernel/k_client_port.h"
#include "core/hle/kernel/k_light_client_session.h"
#include "core/hle/kernel/k_light_server_session.h"
#include "core/hle/kernel/k_light_session.h"
#include "core/hle/kernel/k_process.h"

namespace Kernel {

KLightSession::KLightSession(KernelCore& kernel)
    : KAutoObjectWithSlabHeapAndContainer(kernel)
    , m_server(kernel)
    , m_client(kernel)
{}
KLightSession::~KLightSession() = default;

void KLightSession::Initialize(KernelCore& kernel, KClientPort* client_port, uintptr_t name) {
    // Increment reference count.
    // Because reference count is one on creation, this will result
    // in a reference count of two. Thus, when both server and client are closed
    // this object will be destroyed.
    this->Open(kernel);

    // Create our sub sessions.
    KAutoObject::Create(std::addressof(m_server));
    KAutoObject::Create(std::addressof(m_client));

    // Initialize our sub sessions.
    m_server.Initialize(this);
    m_client.Initialize(this);

    // Set state and name.
    m_state = State::Normal;
    m_name = name;

    // Set our owner process.
    m_process = GetCurrentProcessPointer(kernel);
    m_process->Open(kernel);

    // Set our port.
    m_port = client_port;
    if (m_port != nullptr) {
        m_port->Open(kernel);
    }

    // Mark initialized.
    m_initialized = true;
}

void KLightSession::Finalize(KernelCore& kernel) {
    if (m_port != nullptr) {
        m_port->OnSessionFinalized(kernel);
        m_port->Close(kernel);
    }
}

void KLightSession::OnServerClosed(KernelCore& kernel) {
    if (m_state == State::Normal) {
        m_state = State::ServerClosed;
        m_client.OnServerClosed(kernel);
    }
    this->Close(kernel);
}

void KLightSession::OnClientClosed(KernelCore& kernel) {
    if (m_state == State::Normal) {
        m_state = State::ClientClosed;
        m_server.OnClientClosed(kernel);
    }
    this->Close(kernel);
}

void KLightSession::PostDestroy(KernelCore& kernel, uintptr_t arg) {
    // Release the session count resource the owner process holds.
    KProcess* owner = reinterpret_cast<KProcess*>(arg);
    owner->ReleaseResource(kernel, Svc::LimitableResource::SessionCountMax, 1);
    owner->Close(kernel);
}

} // namespace Kernel
