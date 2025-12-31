// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <boost/container/small_vector.hpp>

#include "common/common_types.h"
#include "core/hardware_properties.h"
#include "core/hle/kernel/k_priority_queue.h"
#include "core/hle/kernel/k_scheduler_lock.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/svc_types.h"

namespace Kernel {

class KernelCore;
class SchedulerLock;

using KSchedulerPriorityQueue =
    KPriorityQueue<KThread, Core::Hardware::NUM_CPU_CORES, Svc::LowestThreadPriority,
                   Svc::HighestThreadPriority>;

static constexpr s32 HighestCoreMigrationAllowedPriority = 2;
static_assert(Svc::LowestThreadPriority >= HighestCoreMigrationAllowedPriority);
static_assert(Svc::HighestThreadPriority <= HighestCoreMigrationAllowedPriority);

class GlobalSchedulerContext final {
    friend class KScheduler;

public:
    static constexpr size_t MAX_THREADS = 256;
    using LockType = KAbstractSchedulerLock<KScheduler>;
    using ThreadList = boost::container::small_vector<KThread*, MAX_THREADS>;

    explicit GlobalSchedulerContext(KernelCore& kernel);
    ~GlobalSchedulerContext();

    /// @brief Returns a list of all threads managed by the scheduler
    /// This is only safe to iterate while holding the scheduler lock
    ThreadList const& GetThreadList() const noexcept {
        return m_thread_list;
    }
    LockType& SchedulerLock() noexcept {
        return m_scheduler_lock;
    }
    void AddThread(KThread* thread) noexcept;
    void RemoveThread(KThread* thread) noexcept;
    void PreemptThreads() noexcept;
    bool IsLocked() const noexcept;
    void UnregisterDummyThreadForWakeup(KThread* thread) noexcept;
    void RegisterDummyThreadForWakeup(KThread* thread) noexcept;
    void WakeupWaitingDummyThreads() noexcept;

private:
    friend class KScopedSchedulerLock;
    friend class KScopedSchedulerLockAndSleep;

    KernelCore& m_kernel;
    std::atomic_bool m_scheduler_update_needed{};
    KSchedulerPriorityQueue m_priority_queue;
    LockType m_scheduler_lock;
    std::mutex m_global_list_guard;
    /// Lists dummy threads pending wakeup on lock release
    ThreadList m_woken_dummy_threads;
    /// Lists all thread ids that aren't deleted/etc.
    ThreadList m_thread_list;
};

} // namespace Kernel
