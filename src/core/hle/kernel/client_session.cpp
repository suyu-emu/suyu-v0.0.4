// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/kernel/session.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/result.h"

namespace Kernel {

ClientSession::ClientSession(KernelCore& kernel) : Object{kernel} {}
ClientSession::~ClientSession() {
    // This destructor will be called automatically when the last ClientSession handle is closed by
    // the emulated application.
    if (auto server = parent->server.lock()) {
        server->ClientDisconnected();
    }
}

ResultCode ClientSession::SendSyncRequest(Thread* thread) {
    // Signal the server session that new data is available
    if (auto server = parent->server.lock()) {
        return server->HandleSyncRequest(SharedFrom(thread));
    }

    return ERR_SESSION_CLOSED_BY_REMOTE;
}

} // namespace Kernel
