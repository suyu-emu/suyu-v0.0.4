// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/kernel/session.h"

namespace Kernel {

Session::Session(KernelCore& kernel) : WaitObject{kernel} {}
Session::~Session() = default;

Session::SessionPair Session::Create(KernelCore& kernel, std::string name) {
    auto session{std::make_shared<Session>(kernel)};
    auto client_session{Kernel::ClientSession::Create(kernel, session, name + "_Client").Unwrap()};
    auto server_session{Kernel::ServerSession::Create(kernel, session, name + "_Server").Unwrap()};

    session->name = std::move(name);
    session->client = client_session;
    session->server = server_session;

    return std::make_pair(std::move(client_session), std::move(server_session));
}

bool Session::ShouldWait(const Thread* thread) const {
    UNIMPLEMENTED();
    return {};
}

void Session::Acquire(Thread* thread) {
    UNIMPLEMENTED();
}

} // namespace Kernel
