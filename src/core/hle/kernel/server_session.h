// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "common/common_types.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/wait_object.h"
#include "core/hle/result.h"

namespace Kernel {

class ClientPort;
class ClientSession;
class HLERequestContext;
class KernelCore;
class ServerSession;
class Session;
class SessionRequestHandler;
class Thread;

/**
 * Kernel object representing the server endpoint of an IPC session. Sessions are the basic CTR-OS
 * primitive for communication between different processes, and are used to implement service calls
 * to the various system services.
 *
 * To make a service call, the client must write the command header and parameters to the buffer
 * located at offset 0x80 of the TLS (Thread-Local Storage) area, then execute a SendSyncRequest
 * SVC call with its ClientSession handle. The kernel will read the command header, using it to
 * marshall the parameters to the process at the server endpoint of the session.
 * After the server replies to the request, the response is marshalled back to the caller's
 * TLS buffer and control is transferred back to it.
 */
class ServerSession final : public WaitObject {
public:
    std::string GetTypeName() const override {
        return "ServerSession";
    }

    static const HandleType HANDLE_TYPE = HandleType::ServerSession;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    using SessionPair = std::tuple<SharedPtr<ServerSession>, SharedPtr<ClientSession>>;

    /**
     * Creates a pair of ServerSession and an associated ClientSession.
     * @param kernel      The kernal instance to create the session pair under.
     * @param name        Optional name of the ports.
     * @param client_port Optional The ClientPort that spawned this session.
     * @return The created session tuple
     */
    static SessionPair CreateSessionPair(KernelCore& kernel, const std::string& name = "Unknown",
                                         SharedPtr<ClientPort> client_port = nullptr);

    /**
     * Sets the HLE handler for the session. This handler will be called to service IPC requests
     * instead of the regular IPC machinery. (The regular IPC machinery is currently not
     * implemented.)
     */
    void SetHleHandler(std::shared_ptr<SessionRequestHandler> hle_handler_) {
        hle_handler = std::move(hle_handler_);
    }

    /**
     * Handle a sync request from the emulated application.
     * @param thread Thread that initiated the request.
     * @returns ResultCode from the operation.
     */
    ResultCode HandleSyncRequest(SharedPtr<Thread> thread);

    bool ShouldWait(Thread* thread) const override;

    void Acquire(Thread* thread) override;

    std::string name;                ///< The name of this session (optional)
    std::shared_ptr<Session> parent; ///< The parent session, which links to the client endpoint.
    std::shared_ptr<SessionRequestHandler>
        hle_handler; ///< This session's HLE request handler (applicable when not a domain)

    /// This is the list of domain request handlers (after conversion to a domain)
    std::vector<std::shared_ptr<SessionRequestHandler>> domain_request_handlers;

    /// List of threads that are pending a response after a sync request. This list is processed in
    /// a LIFO manner, thus, the last request will be dispatched first.
    /// TODO(Subv): Verify if this is indeed processed in LIFO using a hardware test.
    std::vector<SharedPtr<Thread>> pending_requesting_threads;

    /// Thread whose request is currently being handled. A request is considered "handled" when a
    /// response is sent via svcReplyAndReceive.
    /// TODO(Subv): Find a better name for this.
    SharedPtr<Thread> currently_handling;

    /// Returns true if the session has been converted to a domain, otherwise False
    bool IsDomain() const {
        return !IsSession();
    }

    /// Returns true if this session has not been converted to a domain, otherwise false.
    bool IsSession() const {
        return domain_request_handlers.empty();
    }

    /// Converts the session to a domain at the end of the current command
    void ConvertToDomain() {
        convert_to_domain = true;
    }

private:
    explicit ServerSession(KernelCore& kernel);
    ~ServerSession() override;

    /**
     * Creates a server session. The server session can have an optional HLE handler,
     * which will be invoked to handle the IPC requests that this session receives.
     * @param kernel The kernel instance to create this server session under.
     * @param name Optional name of the server session.
     * @return The created server session
     */
    static ResultVal<SharedPtr<ServerSession>> Create(KernelCore& kernel,
                                                      std::string name = "Unknown");

    /// Handles a SyncRequest to a domain, forwarding the request to the proper object or closing an
    /// object handle.
    ResultCode HandleDomainSyncRequest(Kernel::HLERequestContext& context);

    /// When set to True, converts the session to a domain at the end of the command
    bool convert_to_domain{};
};

} // namespace Kernel
