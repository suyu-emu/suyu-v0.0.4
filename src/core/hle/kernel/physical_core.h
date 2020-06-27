// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>

namespace Common {
class SpinLock;
}

namespace Kernel {
class Scheduler;
} // namespace Kernel

namespace Core {
class ARM_Interface;
class CPUInterruptHandler;
class ExclusiveMonitor;
class System;
} // namespace Core

namespace Kernel {

class PhysicalCore {
public:
    PhysicalCore(Core::System& system, std::size_t id, Kernel::Scheduler& scheduler,
                 Core::CPUInterruptHandler& interrupt_handler);
    ~PhysicalCore();

    PhysicalCore(const PhysicalCore&) = delete;
    PhysicalCore& operator=(const PhysicalCore&) = delete;

    PhysicalCore(PhysicalCore&&) = default;
    PhysicalCore& operator=(PhysicalCore&&) = default;

    void Idle();
    /// Interrupt this physical core.
    void Interrupt();

    /// Clear this core's interrupt
    void ClearInterrupt();

    /// Check if this core is interrupted
    bool IsInterrupted() const;

    // Shutdown this physical core.
    void Shutdown();

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
        return scheduler;
    }

    const Kernel::Scheduler& Scheduler() const {
        return scheduler;
    }

private:
    Core::CPUInterruptHandler& interrupt_handler;
    std::size_t core_index;
    Kernel::Scheduler& scheduler;
    std::unique_ptr<Common::SpinLock> guard;
};

} // namespace Kernel
