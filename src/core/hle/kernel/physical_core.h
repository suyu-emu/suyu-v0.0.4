// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>

#include "core/arm/cpu_interrupt_handler.h"

namespace Common {
class SpinLock;
}

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
    PhysicalCore(Core::System& system, std::size_t id, Core::ExclusiveMonitor& exclusive_monitor,
                 Core::CPUInterruptHandler& interrupt_handler, Core::ARM_Interface& arm_interface32,
                 Core::ARM_Interface& arm_interface64);
    ~PhysicalCore();

    PhysicalCore(const PhysicalCore&) = delete;
    PhysicalCore& operator=(const PhysicalCore&) = delete;

    PhysicalCore(PhysicalCore&&) = default;
    PhysicalCore& operator=(PhysicalCore&&) = default;

    /// Execute current jit state
    void Run();
    /// Clear Exclusive state.
    void ClearExclusive();
    /// Set this core in IdleState.
    void Idle();
    /// Execute a single instruction in current jit.
    void Step();
    /// Stop JIT execution/exit
    void Stop();

    /// Interrupt this physical core.
    void Interrupt();

    /// Clear this core's interrupt
    void ClearInterrupt();

    /// Check if this core is interrupted
    bool IsInterrupted() const {
        return interrupt_handler.IsInterrupted();
    }

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

    void SetIs64Bit(bool is_64_bit);

private:
    Core::CPUInterruptHandler& interrupt_handler;
    std::size_t core_index;
    Core::ARM_Interface& arm_interface_32;
    Core::ARM_Interface& arm_interface_64;
    std::unique_ptr<Kernel::Scheduler> scheduler;
    Core::ARM_Interface* arm_interface{};
    std::unique_ptr<Common::SpinLock> guard;
};

} // namespace Kernel
