// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/kernel/object.h"

namespace Kernel {

class ClientSession;
class ClientPort;
class ServerSession;

/**
 * Parent structure to link the client and server endpoints of a session with their associated
 * client port. The client port need not exist, as is the case for portless sessions like the
 * FS File and Directory sessions. When one of the endpoints of a session is destroyed, its
 * corresponding field in this structure will be set to nullptr.
 */
class Session final {
public:
    std::weak_ptr<ClientSession> client; ///< The client endpoint of the session.
    std::weak_ptr<ServerSession> server; ///< The server endpoint of the session.
    std::shared_ptr<ClientPort> port; ///< The port that this session is associated with (optional).
};
} // namespace Kernel
