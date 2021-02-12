// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/kernel/session.h"
#include "core/hle/kernel/svc_results.h"
#include "core/hle/result.h"

namespace Kernel {

ClientSession::ClientSession(KernelCore& kernel) : KSynchronizationObject{kernel} {}

ClientSession::~ClientSession() {
    // This destructor will be called automatically when the last ClientSession handle is closed by
    // the emulated application.
    if (parent->Server()) {
        parent->Server()->ClientDisconnected();
    }
}

bool ClientSession::IsSignaled() const {
    UNIMPLEMENTED();
    return true;
}

ResultVal<std::shared_ptr<ClientSession>> ClientSession::Create(KernelCore& kernel,
                                                                std::shared_ptr<Session> parent,
                                                                std::string name) {
    std::shared_ptr<ClientSession> client_session{std::make_shared<ClientSession>(kernel)};

    client_session->name = std::move(name);
    client_session->parent = std::move(parent);

    return MakeResult(std::move(client_session));
}

ResultCode ClientSession::SendSyncRequest(std::shared_ptr<KThread> thread,
                                          Core::Memory::Memory& memory,
                                          Core::Timing::CoreTiming& core_timing) {
    // Keep ServerSession alive until we're done working with it.
    if (!parent->Server()) {
        return ResultSessionClosedByRemote;
    }

    // Signal the server session that new data is available
    return parent->Server()->HandleSyncRequest(std::move(thread), memory, core_timing);
}

} // namespace Kernel
