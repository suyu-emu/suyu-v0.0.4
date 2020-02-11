// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <utility>

#include "core/hle/kernel/object.h"
#include "core/hle/result.h"

namespace Core {
class System;
} // namespace Core

namespace Kernel {

class KernelCore;
class SynchronizationObject;

class Synchronization {
public:
    Synchronization(Core::System& system);

    void SignalObject(SynchronizationObject& obj) const;

    std::pair<ResultCode, Handle> WaitFor(
        std::vector<std::shared_ptr<SynchronizationObject>>& sync_objects, s64 nano_seconds);

private:
    Core::System& system;
};
} // namespace Kernel
