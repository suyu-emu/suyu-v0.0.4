// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace Kernel {
class Scheduler;
} // namespace Kernel

class ARM_Interface;
class ExclusiveMonitor;

namespace Kernel {

class PhysicalCore {
public:
    PhysicalCore(KernelCore& kernel, std::size_t id, ExclusiveMonitor& exclusive_monitor);

    /// Execute current jit state
    void Run();
    /// Execute a single instruction in current jit.
    void Step();
    /// Stop JIT execution/exit
    void Stop();

    ARM_Interface& ArmInterface() {
        return *arm_interface;
    }

    const ARM_Interface& ArmInterface() const {
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

    Scheduler& Scheduler() {
        return *scheduler;
    }

    const Scheduler& Scheduler() const {
        return *scheduler;
    }

private:
    std::size_t core_index;
    std::unique_ptr<ARM_Interface> arm_interface;
    std::unique_ptr<Kernel::Scheduler> scheduler;
    KernelCore& kernel;
}

} // namespace Kernel
