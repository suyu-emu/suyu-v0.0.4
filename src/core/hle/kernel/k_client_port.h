// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include "common/common_types.h"
#include "core/hle/kernel/k_synchronization_object.h"
#include "core/hle/result.h"

namespace Kernel {

class KClientSession;
class KernelCore;
class ServerPort;

class KClientPort final : public KSynchronizationObject {
    KERNEL_AUTOOBJECT_TRAITS(KClientPort, KSynchronizationObject);

public:
    explicit KClientPort(KernelCore& kernel);
    virtual ~KClientPort() override;

    friend class ServerPort;

    void Initialize(s32 max_sessions_, std::string&& name_);

    std::shared_ptr<ServerPort> GetServerPort() const;

    /**
     * Creates a new Session pair, adds the created ServerSession to the associated ServerPort's
     * list of pending sessions, and signals the ServerPort, causing any threads
     * waiting on it to awake.
     * @returns ClientSession The client endpoint of the created Session pair, or error code.
     */
    ResultVal<KClientSession*> Connect();

    /**
     * Signifies that a previously active connection has been closed,
     * decreasing the total number of active connections to this port.
     */
    void ConnectionClosed();

    // Overridden virtual functions.
    virtual void Destroy() override;
    virtual bool IsSignaled() const override;

    // DEPRECATED

    std::string GetTypeName() const override {
        return "ClientPort";
    }
    std::string GetName() const override {
        return name;
    }

    static constexpr HandleType HANDLE_TYPE = HandleType::ClientPort;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

private:
    std::shared_ptr<ServerPort> server_port; ///< ServerPort associated with this client port.
    s32 max_sessions = 0; ///< Maximum number of simultaneous sessions the port can have
    std::atomic<s32> num_sessions = 0; ///< Number of currently open sessions to this port
    std::string name;                  ///< Name of client port (optional)
};

} // namespace Kernel
