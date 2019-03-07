// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include "core/hle/kernel/object.h"

union ResultCode;

namespace Kernel {

class KernelCore;
class Session;
class ServerSession;
class Thread;

class ClientSession final : public Object {
public:
    friend class ServerSession;

    std::string GetTypeName() const override {
        return "ClientSession";
    }

    std::string GetName() const override {
        return name;
    }

    static const HandleType HANDLE_TYPE = HandleType::ClientSession;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    ResultCode SendSyncRequest(SharedPtr<Thread> thread);

private:
    explicit ClientSession(KernelCore& kernel);
    ~ClientSession() override;

    /// The parent session, which links to the server endpoint.
    std::shared_ptr<Session> parent;

    /// Name of the client session (optional)
    std::string name;
};

} // namespace Kernel
