// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/k_scoped_resource_reservation.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/kernel/session.h"

namespace Kernel {

Session::Session(KernelCore& kernel) : KSynchronizationObject{kernel} {}
Session::~Session() {
    // Release reserved resource when the Session pair was created.
    kernel.GetSystemResourceLimit()->Release(LimitableResource::Sessions, 1);
}

Session::SessionPair Session::Create(KernelCore& kernel, std::string name) {
    // Reserve a new session from the resource limit.
    KScopedResourceReservation session_reservation(kernel.GetSystemResourceLimit(),
                                                   LimitableResource::Sessions);
    ASSERT(session_reservation.Succeeded());
    auto session{std::make_shared<Session>(kernel)};
    auto client_session{Kernel::ClientSession::Create(kernel, session, name + "_Client").Unwrap()};
    auto server_session{Kernel::ServerSession::Create(kernel, session, name + "_Server").Unwrap()};

    session->name = std::move(name);
    session->client = client_session;
    session->server = server_session;

    session_reservation.Commit();
    return std::make_pair(std::move(client_session), std::move(server_session));
}

bool Session::IsSignaled() const {
    UNIMPLEMENTED();
    return true;
}

} // namespace Kernel
