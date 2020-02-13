// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "core/hle/kernel/object.h"
#include "core/hle/result.h"

namespace Core {
class System;
} // namespace Core

namespace Kernel {

class SynchronizationObject;

/**
 * The 'Synchronization' class is an interface for handling synchronization methods
 * used by Synchronization objects and synchronization SVCs. This centralizes processing of
 * such
 */
class Synchronization {
public:
    explicit Synchronization(Core::System& system);

    /// Signals a synchronization object, waking up all its waiting threads
    void SignalObject(SynchronizationObject& obj) const;

    /// Tries to see if waiting for any of the sync_objects is necessary, if not
    /// it returns Success and the handle index of the signaled sync object. In
    /// case not, the current thread will be locked and wait for nano_seconds or
    /// for a synchronization object to signal.
    std::pair<ResultCode, Handle> WaitFor(
        std::vector<std::shared_ptr<SynchronizationObject>>& sync_objects, s64 nano_seconds);

private:
    Core::System& system;
};
} // namespace Kernel
