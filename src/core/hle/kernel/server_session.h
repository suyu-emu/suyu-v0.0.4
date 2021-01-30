// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "common/threadsafe_queue.h"
#include "core/hle/kernel/k_synchronization_object.h"
#include "core/hle/kernel/service_thread.h"
#include "core/hle/result.h"

namespace Core::Memory {
class Memory;
}

namespace Core::Timing {
class CoreTiming;
struct EventType;
} // namespace Core::Timing

namespace Kernel {

class HLERequestContext;
class KernelCore;
class Session;
class SessionRequestHandler;
class KThread;

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
class ServerSession final : public KSynchronizationObject {
    friend class ServiceThread;

public:
    explicit ServerSession(KernelCore& kernel);
    ~ServerSession() override;

    friend class Session;

    static ResultVal<std::shared_ptr<ServerSession>> Create(KernelCore& kernel,
                                                            std::shared_ptr<Session> parent,
                                                            std::string name = "Unknown");

    std::string GetTypeName() const override {
        return "ServerSession";
    }

    std::string GetName() const override {
        return name;
    }

    static constexpr HandleType HANDLE_TYPE = HandleType::ServerSession;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    Session* GetParent() {
        return parent.get();
    }

    const Session* GetParent() const {
        return parent.get();
    }

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
     *
     * @param thread      Thread that initiated the request.
     * @param memory      Memory context to handle the sync request under.
     * @param core_timing Core timing context to schedule the request event under.
     *
     * @returns ResultCode from the operation.
     */
    ResultCode HandleSyncRequest(std::shared_ptr<KThread> thread, Core::Memory::Memory& memory,
                                 Core::Timing::CoreTiming& core_timing);

    /// Called when a client disconnection occurs.
    void ClientDisconnected();

    /// Adds a new domain request handler to the collection of request handlers within
    /// this ServerSession instance.
    void AppendDomainRequestHandler(std::shared_ptr<SessionRequestHandler> handler);

    /// Retrieves the total number of domain request handlers that have been
    /// appended to this ServerSession instance.
    std::size_t NumDomainRequestHandlers() const;

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

    bool IsSignaled() const override;

    void Finalize() override {}

private:
    /// Queues a sync request from the emulated application.
    ResultCode QueueSyncRequest(std::shared_ptr<KThread> thread, Core::Memory::Memory& memory);

    /// Completes a sync request from the emulated application.
    ResultCode CompleteSyncRequest(HLERequestContext& context);

    /// Handles a SyncRequest to a domain, forwarding the request to the proper object or closing an
    /// object handle.
    ResultCode HandleDomainSyncRequest(Kernel::HLERequestContext& context);

    /// The parent session, which links to the client endpoint.
    std::shared_ptr<Session> parent;

    /// This session's HLE request handler (applicable when not a domain)
    std::shared_ptr<SessionRequestHandler> hle_handler;

    /// This is the list of domain request handlers (after conversion to a domain)
    std::vector<std::shared_ptr<SessionRequestHandler>> domain_request_handlers;

    /// List of threads that are pending a response after a sync request. This list is processed in
    /// a LIFO manner, thus, the last request will be dispatched first.
    /// TODO(Subv): Verify if this is indeed processed in LIFO using a hardware test.
    std::vector<std::shared_ptr<KThread>> pending_requesting_threads;

    /// Thread whose request is currently being handled. A request is considered "handled" when a
    /// response is sent via svcReplyAndReceive.
    /// TODO(Subv): Find a better name for this.
    std::shared_ptr<KThread> currently_handling;

    /// When set to True, converts the session to a domain at the end of the command
    bool convert_to_domain{};

    /// The name of this session (optional)
    std::string name;

    /// Thread to dispatch service requests
    std::weak_ptr<ServiceThread> service_thread;
};

} // namespace Kernel
