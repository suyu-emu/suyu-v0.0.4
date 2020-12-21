// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <vector>

#include "common/common_types.h"
#include "common/spin_lock.h"
#include "core/hardware_properties.h"
#include "core/hle/kernel/k_priority_queue.h"
#include "core/hle/kernel/k_scheduler_lock.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {

class KernelCore;
class SchedulerLock;

using KSchedulerPriorityQueue =
    KPriorityQueue<Thread, Core::Hardware::NUM_CPU_CORES, THREADPRIO_LOWEST, THREADPRIO_HIGHEST>;
constexpr s32 HighestCoreMigrationAllowedPriority = 2;

class GlobalSchedulerContext final {
    friend class KScheduler;

public:
    using LockType = KAbstractSchedulerLock<KScheduler>;

    explicit GlobalSchedulerContext(KernelCore& kernel);
    ~GlobalSchedulerContext();

    /// Adds a new thread to the scheduler
    void AddThread(std::shared_ptr<Thread> thread);

    /// Removes a thread from the scheduler
    void RemoveThread(std::shared_ptr<Thread> thread);

    /// Returns a list of all threads managed by the scheduler
    [[nodiscard]] const std::vector<std::shared_ptr<Thread>>& GetThreadList() const {
        return thread_list;
    }

    /**
     * Rotates the scheduling queues of threads at a preemption priority and then does
     * some core rebalancing. Preemption priorities can be found in the array
     * 'preemption_priorities'.
     *
     * @note This operation happens every 10ms.
     */
    void PreemptThreads();

    /// Returns true if the global scheduler lock is acquired
    bool IsLocked() const;

    [[nodiscard]] LockType& SchedulerLock() {
        return scheduler_lock;
    }

    [[nodiscard]] const LockType& SchedulerLock() const {
        return scheduler_lock;
    }

private:
    friend class KScopedSchedulerLock;
    friend class KScopedSchedulerLockAndSleep;

    KernelCore& kernel;

    std::atomic_bool scheduler_update_needed{};
    KSchedulerPriorityQueue priority_queue;
    LockType scheduler_lock;

    /// Lists all thread ids that aren't deleted/etc.
    std::vector<std::shared_ptr<Thread>> thread_list;
    Common::SpinLock global_list_guard{};
};

} // namespace Kernel
