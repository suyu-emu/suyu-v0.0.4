// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <utility>

#include "core/hle/kernel/k_synchronization_object.h"

namespace Kernel {

class ClientSession;
class ServerSession;

/**
 * Parent structure to link the client and server endpoints of a session with their associated
 * client port.
 */
class Session final : public KSynchronizationObject {
public:
    explicit Session(KernelCore& kernel);
    ~Session() override;

    using SessionPair = std::pair<std::shared_ptr<ClientSession>, std::shared_ptr<ServerSession>>;

    static SessionPair Create(KernelCore& kernel, std::string name = "Unknown");

    std::string GetName() const override {
        return name;
    }

    static constexpr HandleType HANDLE_TYPE = HandleType::Session;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    bool IsSignaled() const override;

    void Finalize() override {}

    std::shared_ptr<ClientSession> Client() {
        if (auto result{client.lock()}) {
            return result;
        }
        return {};
    }

    std::shared_ptr<ServerSession> Server() {
        if (auto result{server.lock()}) {
            return result;
        }
        return {};
    }

private:
    std::string name;
    std::weak_ptr<ClientSession> client;
    std::weak_ptr<ServerSession> server;
};

} // namespace Kernel
