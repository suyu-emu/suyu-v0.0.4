// Copyright 2020 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>

namespace Kernel {

class HLERequestContext;
class KernelCore;
class ServerSession;

class ServiceThread final {
public:
    explicit ServiceThread(KernelCore& kernel);
    ~ServiceThread();

    void QueueSyncRequest(ServerSession& session, std::shared_ptr<HLERequestContext>&& context);

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace Kernel
