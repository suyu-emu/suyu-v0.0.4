// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>

namespace Kernel {
class Scheduler;
} // namespace Kernel

namespace Core {
class ARM_Interface;
class ExclusiveMonitor;
class System;
} // namespace Core

namespace Kernel {

class PhysicalCore {
public:
    PhysicalCore(Core::System& system, std::size_t id, Core::ExclusiveMonitor& exclusive_monitor);
    ~PhysicalCore();

    PhysicalCore(const PhysicalCore&) = delete;
    PhysicalCore& operator=(const PhysicalCore&) = delete;

    PhysicalCore(PhysicalCore&&) = default;
    PhysicalCore& operator=(PhysicalCore&&) = default;

    /// Execute current jit state
    void Run();
    /// Execute a single instruction in current jit.
    void Step();
    /// Stop JIT execution/exit
    void Stop();

    // Shutdown this physical core.
    void Shutdown();

    Core::ARM_Interface& ArmInterface() {
        return *arm_interface;
    }

    const Core::ARM_Interface& ArmInterface() const {
        return *arm_interface;
    }

    bool IsMainCore() const {
        return core_index == 0;
    }

    bool IsSystemCore() const {
        return core_index == 3;
    }

    std::size_t CoreIndex() const {
        return core_index;
    }

    Kernel::Scheduler& Scheduler() {
        return *scheduler;
    }

    const Kernel::Scheduler& Scheduler() const {
        return *scheduler;
    }

private:
    std::size_t core_index;
    std::unique_ptr<Core::ARM_Interface> arm_interface;
    std::unique_ptr<Kernel::Scheduler> scheduler;
};

} // namespace Kernel
