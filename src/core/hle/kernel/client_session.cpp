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
    if (parent->server) {
        parent->server->ClientDisconnected();
    }

    parent->client = nullptr;
}

ResultCode ClientSession::SendSyncRequest(Thread* thread) {
    // Keep ServerSession alive until we're done working with it.
    if (parent->server == nullptr)
        return ERR_SESSION_CLOSED_BY_REMOTE;

    // Signal the server session that new data is available
    return parent->server->HandleSyncRequest(SharedFrom(thread));
}

} // namespace Kernel
