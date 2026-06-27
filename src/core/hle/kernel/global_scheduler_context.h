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
    using LockType = KAbstractSchedulerLock<KScheduler>;

    explicit GlobalSchedulerContext(KernelCore& kernel);
    ~GlobalSchedulerContext();

    void AddThread(KThread* thread);
    void RemoveThread(KThread* thread);

    const boost::container::small_vector<KThread*, 256>& GetThreadList() const {
        return m_thread_list;
    }

    void PreemptThreads();

    /// Returns true if the global scheduler lock is acquired
    bool IsLocked() const;

    void UnregisterDummyThreadForWakeup(KThread* thread);
    void RegisterDummyThreadForWakeup(KThread* thread);
    void WakeupWaitingDummyThreads();

    LockType& SchedulerLock() {
        return m_scheduler_lock;
    }

private:
    friend class KScopedSchedulerLock;
    friend class KScopedSchedulerLockAndSleep;

    KernelCore& m_kernel;

    std::atomic_bool m_scheduler_update_needed{};
    KSchedulerPriorityQueue m_priority_queue;
    LockType m_scheduler_lock;

    boost::container::small_vector<KThread*, 256> m_woken_dummy_threads;
    boost::container::small_vector<KThread*, 256> m_thread_list;
    std::mutex m_global_list_guard;
};

} // namespace Kernel
