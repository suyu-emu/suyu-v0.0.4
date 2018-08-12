// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include "common/common_types.h"
#include "core/arm/exclusive_monitor.h"

class ARM_Interface;

namespace Kernel {
class Scheduler;
}

namespace Core {

constexpr unsigned NUM_CPU_CORES{4};

class CpuBarrier {
public:
    bool IsAlive() const {
        return !end;
    }

    void NotifyEnd();

    bool Rendezvous();

private:
    unsigned cores_waiting{NUM_CPU_CORES};
    std::mutex mutex;
    std::condition_variable condition;
    std::atomic<bool> end{};
};

class Cpu {
public:
    Cpu(std::shared_ptr<ExclusiveMonitor> exclusive_monitor,
        std::shared_ptr<CpuBarrier> cpu_barrier, size_t core_index);

    void RunLoop(bool tight_loop = true);

    void SingleStep();

    void PrepareReschedule();

    ARM_Interface& ArmInterface() {
        return *arm_interface;
    }

    const ARM_Interface& ArmInterface() const {
        return *arm_interface;
    }

    const std::shared_ptr<Kernel::Scheduler>& Scheduler() const {
        return scheduler;
    }

    bool IsMainCore() const {
        return core_index == 0;
    }

    size_t CoreIndex() const {
        return core_index;
    }

    static std::shared_ptr<ExclusiveMonitor> MakeExclusiveMonitor(size_t num_cores);

private:
    void Reschedule();

    std::shared_ptr<ARM_Interface> arm_interface;
    std::shared_ptr<CpuBarrier> cpu_barrier;
    std::shared_ptr<Kernel::Scheduler> scheduler;

    std::atomic<bool> reschedule_pending = false;
    size_t core_index;
};

} // namespace Core
