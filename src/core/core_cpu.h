// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include "common/common_types.h"

namespace Kernel {
class GlobalScheduler;
class PhysicalCore;
} // namespace Kernel

namespace Core {
class System;
}

namespace Core::Timing {
class CoreTiming;
}

namespace Memory {
class Memory;
}

namespace Core {

constexpr unsigned NUM_CPU_CORES{4};

class Cpu {
public:
    Cpu(System& system, std::size_t core_index);
    ~Cpu();

    void RunLoop(bool tight_loop = true);

    void SingleStep();

    void PrepareReschedule();

    bool IsMainCore() const {
        return core_index == 0;
    }

    std::size_t CoreIndex() const {
        return core_index;
    }

private:
    void Reschedule();

    Kernel::GlobalScheduler& global_scheduler;
    Kernel::PhysicalCore& physical_core;
    Timing::CoreTiming& core_timing;

    std::atomic<bool> reschedule_pending = false;
    std::size_t core_index;
};

} // namespace Core
