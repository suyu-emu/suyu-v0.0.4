// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include "core/hle/kernel/wait_object.h"
#include "core/hle/result.h"

union ResultCode;

namespace Memory {
class Memory;
}

namespace Kernel {

class KernelCore;
class Session;
class Thread;

class ClientSession final : public WaitObject {
public:
    explicit ClientSession(KernelCore& kernel);
    ~ClientSession() override;

    friend class Session;

    std::string GetTypeName() const override {
        return "ClientSession";
    }

    std::string GetName() const override {
        return name;
    }

    static constexpr HandleType HANDLE_TYPE = HandleType::ClientSession;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    ResultCode SendSyncRequest(std::shared_ptr<Thread> thread, Memory::Memory& memory);

    bool ShouldWait(const Thread* thread) const override;

    void Acquire(Thread* thread) override;

private:
    static ResultVal<std::shared_ptr<ClientSession>> Create(KernelCore& kernel,
                                                            std::shared_ptr<Session> parent,
                                                            std::string name = "Unknown");

    /// The parent session, which links to the server endpoint.
    std::shared_ptr<Session> parent;

    /// Name of the client session (optional)
    std::string name;
};

} // namespace Kernel
