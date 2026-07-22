// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/kernel/k_light_client_session.h"
#include "core/hle/kernel/k_light_session.h"
#include "core/hle/kernel/k_thread.h"

namespace Kernel {

KLightClientSession::KLightClientSession(KernelCore& kernel) : KAutoObject(kernel) {}

KLightClientSession::~KLightClientSession() = default;

void KLightClientSession::Destroy(KernelCore& kernel) {
    m_parent->OnClientClosed(kernel);
}

void KLightClientSession::OnServerClosed(KernelCore& kernel) {}

Result KLightClientSession::SendSyncRequest(KernelCore& kernel, u32* data) {
    // Get the request thread.
    KThread* cur_thread = GetCurrentThreadPointer(kernel);

    // Set the light data.
    cur_thread->SetLightSessionData(data);

    // Send the request.
    R_RETURN(m_parent->OnRequest(kernel, cur_thread));
}

} // namespace Kernel
