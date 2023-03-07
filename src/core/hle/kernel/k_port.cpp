// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/kernel/k_port.h"
#include "core/hle/kernel/k_scheduler.h"
#include "core/hle/kernel/svc_results.h"

namespace Kernel {

KPort::KPort(KernelCore& kernel_)
    : KAutoObjectWithSlabHeapAndContainer{kernel_}, m_server{kernel_}, m_client{kernel_} {}

KPort::~KPort() = default;

void KPort::Initialize(s32 max_sessions, bool is_light, uintptr_t name) {
    // Open a new reference count to the initialized port.
    this->Open();

    // Create and initialize our server/client pair.
    KAutoObject::Create(std::addressof(m_server));
    KAutoObject::Create(std::addressof(m_client));
    m_server.Initialize(this);
    m_client.Initialize(this, max_sessions);

    // Set our member variables.
    m_is_light = is_light;
    m_name = name;
    m_state = State::Normal;
}

void KPort::OnClientClosed() {
    KScopedSchedulerLock sl{kernel};

    if (m_state == State::Normal) {
        m_state = State::ClientClosed;
    }
}

void KPort::OnServerClosed() {
    KScopedSchedulerLock sl{kernel};

    if (m_state == State::Normal) {
        m_state = State::ServerClosed;
    }
}

bool KPort::IsServerClosed() const {
    KScopedSchedulerLock sl{kernel};
    return m_state == State::ServerClosed;
}

Result KPort::EnqueueSession(KServerSession* session) {
    KScopedSchedulerLock sl{kernel};

    R_UNLESS(m_state == State::Normal, ResultPortClosed);

    m_server.EnqueueSession(session);
    R_SUCCEED();
}

} // namespace Kernel
