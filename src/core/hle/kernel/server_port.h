// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "common/common_types.h"
#include "core/hle/kernel/k_synchronization_object.h"
#include "core/hle/kernel/object.h"
#include "core/hle/result.h"

namespace Kernel {

class ClientPort;
class KernelCore;
class ServerSession;
class SessionRequestHandler;

class ServerPort final : public KSynchronizationObject {
public:
    explicit ServerPort(KernelCore& kernel);
    ~ServerPort() override;

    using HLEHandler = std::shared_ptr<SessionRequestHandler>;
    using PortPair = std::pair<std::shared_ptr<ServerPort>, std::shared_ptr<ClientPort>>;

    /**
     * Creates a pair of ServerPort and an associated ClientPort.
     *
     * @param kernel The kernel instance to create the port pair under.
     * @param max_sessions Maximum number of sessions to the port
     * @param name Optional name of the ports
     * @return The created port tuple
     */
    static PortPair CreatePortPair(KernelCore& kernel, u32 max_sessions,
                                   std::string name = "UnknownPort");

    std::string GetTypeName() const override {
        return "ServerPort";
    }
    std::string GetName() const override {
        return name;
    }

    static constexpr HandleType HANDLE_TYPE = HandleType::ServerPort;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    /**
     * Accepts a pending incoming connection on this port. If there are no pending sessions, will
     * return ERR_NO_PENDING_SESSIONS.
     */
    ResultVal<std::shared_ptr<ServerSession>> Accept();

    /// Whether or not this server port has an HLE handler available.
    bool HasHLEHandler() const {
        return hle_handler != nullptr;
    }

    /// Gets the HLE handler for this port.
    HLEHandler GetHLEHandler() const {
        return hle_handler;
    }

    /**
     * Sets the HLE handler template for the port. ServerSessions crated by connecting to this port
     * will inherit a reference to this handler.
     */
    void SetHleHandler(HLEHandler hle_handler_) {
        hle_handler = std::move(hle_handler_);
    }

    /// Appends a ServerSession to the collection of ServerSessions
    /// waiting to be accepted by this port.
    void AppendPendingSession(std::shared_ptr<ServerSession> pending_session);

    bool IsSignaled() const override;

    void Finalize() override {}

private:
    /// ServerSessions waiting to be accepted by the port
    std::vector<std::shared_ptr<ServerSession>> pending_sessions;

    /// This session's HLE request handler template (optional)
    /// ServerSessions created from this port inherit a reference to this handler.
    HLEHandler hle_handler;

    /// Name of the port (optional)
    std::string name;
};

} // namespace Kernel
