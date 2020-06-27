// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include "common/common_types.h"
#include "common/multi_level_queue.h"
#include "common/spin_lock.h"
#include "core/hardware_properties.h"
#include "core/hle/kernel/thread.h"

namespace Common {
class Fiber;
}

namespace Core {
class ARM_Interface;
class System;
} // namespace Core

namespace Kernel {

class KernelCore;
class Process;
class SchedulerLock;

class GlobalScheduler final {
public:
    explicit GlobalScheduler(KernelCore& kernel);
    ~GlobalScheduler();

    /// Adds a new thread to the scheduler
    void AddThread(std::shared_ptr<Thread> thread);

    /// Removes a thread from the scheduler
    void RemoveThread(std::shared_ptr<Thread> thread);

    /// Returns a list of all threads managed by the scheduler
    const std::vector<std::shared_ptr<Thread>>& GetThreadList() const {
        return thread_list;
    }

    /// Notify the scheduler a thread's status has changed.
    void AdjustSchedulingOnStatus(Thread* thread, u32 old_flags);

    /// Notify the scheduler a thread's priority has changed.
    void AdjustSchedulingOnPriority(Thread* thread, u32 old_priority);

    /// Notify the scheduler a thread's core and/or affinity mask has changed.
    void AdjustSchedulingOnAffinity(Thread* thread, u64 old_affinity_mask, s32 old_core);

    /**
     * Takes care of selecting the new scheduled threads in three steps:
     *
     * 1. First a thread is selected from the top of the priority queue. If no thread
     *    is obtained then we move to step two, else we are done.
     *
     * 2. Second we try to get a suggested thread that's not assigned to any core or
     *    that is not the top thread in that core.
     *
     * 3. Third is no suggested thread is found, we do a second pass and pick a running
     *    thread in another core and swap it with its current thread.
     *
     * returns the cores needing scheduling.
     */
    u32 SelectThreads();

    bool HaveReadyThreads(std::size_t core_id) const {
        return !scheduled_queue[core_id].empty();
    }

    /**
     * Takes a thread and moves it to the back of the it's priority list.
     *
     * @note This operation can be redundant and no scheduling is changed if marked as so.
     */
    bool YieldThread(Thread* thread);

    /**
     * Takes a thread and moves it to the back of the it's priority list.
     * Afterwards, tries to pick a suggested thread from the suggested queue that has worse time or
     * a better priority than the next thread in the core.
     *
     * @note This operation can be redundant and no scheduling is changed if marked as so.
     */
    bool YieldThreadAndBalanceLoad(Thread* thread);

    /**
     * Takes a thread and moves it out of the scheduling queue.
     * and into the suggested queue. If no thread can be scheduled afterwards in that core,
     * a suggested thread is obtained instead.
     *
     * @note This operation can be redundant and no scheduling is changed if marked as so.
     */
    bool YieldThreadAndWaitForLoadBalancing(Thread* thread);

    /**
     * Rotates the scheduling queues of threads at a preemption priority and then does
     * some core rebalancing. Preemption priorities can be found in the array
     * 'preemption_priorities'.
     *
     * @note This operation happens every 10ms.
     */
    void PreemptThreads();

    u32 CpuCoresCount() const {
        return Core::Hardware::NUM_CPU_CORES;
    }

    void SetReselectionPending() {
        is_reselection_pending.store(true, std::memory_order_release);
    }

    bool IsReselectionPending() const {
        return is_reselection_pending.load(std::memory_order_acquire);
    }

    void Shutdown();

private:
    friend class SchedulerLock;

    /// Lock the scheduler to the current thread.
    void Lock();

    /// Unlocks the scheduler, reselects threads, interrupts cores for rescheduling
    /// and reschedules current core if needed.
    void Unlock();

    void EnableInterruptAndSchedule(u32 cores_pending_reschedule,
                                    Core::EmuThreadHandle global_thread);

    /**
     * Add a thread to the suggested queue of a cpu core. Suggested threads may be
     * picked if no thread is scheduled to run on the core.
     */
    void Suggest(u32 priority, std::size_t core, Thread* thread);

    /**
     * Remove a thread to the suggested queue of a cpu core. Suggested threads may be
     * picked if no thread is scheduled to run on the core.
     */
    void Unsuggest(u32 priority, std::size_t core, Thread* thread);

    /**
     * Add a thread to the scheduling queue of a cpu core. The thread is added at the
     * back the queue in its priority level.
     */
    void Schedule(u32 priority, std::size_t core, Thread* thread);

    /**
     * Add a thread to the scheduling queue of a cpu core. The thread is added at the
     * front the queue in its priority level.
     */
    void SchedulePrepend(u32 priority, std::size_t core, Thread* thread);

    /// Reschedule an already scheduled thread based on a new priority
    void Reschedule(u32 priority, std::size_t core, Thread* thread);

    /// Unschedules a thread.
    void Unschedule(u32 priority, std::size_t core, Thread* thread);

    /**
     * Transfers a thread into an specific core. If the destination_core is -1
     * it will be unscheduled from its source code and added into its suggested
     * queue.
     */
    void TransferToCore(u32 priority, s32 destination_core, Thread* thread);

    bool AskForReselectionOrMarkRedundant(Thread* current_thread, const Thread* winner);

    static constexpr u32 min_regular_priority = 2;
    std::array<Common::MultiLevelQueue<Thread*, THREADPRIO_COUNT>, Core::Hardware::NUM_CPU_CORES>
        scheduled_queue;
    std::array<Common::MultiLevelQueue<Thread*, THREADPRIO_COUNT>, Core::Hardware::NUM_CPU_CORES>
        suggested_queue;
    std::atomic<bool> is_reselection_pending{false};

    // The priority levels at which the global scheduler preempts threads every 10 ms. They are
    // ordered from Core 0 to Core 3.
    std::array<u32, Core::Hardware::NUM_CPU_CORES> preemption_priorities = {59, 59, 59, 62};

    /// Scheduler lock mechanisms.
    bool is_locked{};
    Common::SpinLock inner_lock{};
    std::atomic<s64> scope_lock{};
    Core::EmuThreadHandle current_owner{Core::EmuThreadHandle::InvalidHandle()};

    Common::SpinLock global_list_guard{};

    /// Lists all thread ids that aren't deleted/etc.
    std::vector<std::shared_ptr<Thread>> thread_list;
    KernelCore& kernel;
};

class Scheduler final {
public:
    explicit Scheduler(Core::System& system, std::size_t core_id);
    ~Scheduler();

    /// Returns whether there are any threads that are ready to run.
    bool HaveReadyThreads() const;

    /// Reschedules to the next available thread (call after current thread is suspended)
    void TryDoContextSwitch();

    /// The next two are for SingleCore Only.
    /// Unload current thread before preempting core.
    void Unload();
    /// Reload current thread after core preemption.
    void Reload();

    /// Gets the current running thread
    Thread* GetCurrentThread() const;

    /// Gets the currently selected thread from the top of the multilevel queue
    Thread* GetSelectedThread() const;

    /// Gets the timestamp for the last context switch in ticks.
    u64 GetLastContextSwitchTicks() const;

    bool ContextSwitchPending() const {
        return is_context_switch_pending;
    }

    void Initialize();

    /// Shutdowns the scheduler.
    void Shutdown();

    void OnThreadStart();

    std::shared_ptr<Common::Fiber>& ControlContext() {
        return switch_fiber;
    }

    const std::shared_ptr<Common::Fiber>& ControlContext() const {
        return switch_fiber;
    }

private:
    friend class GlobalScheduler;

    /// Switches the CPU's active thread context to that of the specified thread
    void SwitchContext();

    /// When a thread wakes up, it must run this through it's new scheduler
    void SwitchContextStep2();

    /**
     * Called on every context switch to update the internal timestamp
     * This also updates the running time ticks for the given thread and
     * process using the following difference:
     *
     * ticks += most_recent_ticks - last_context_switch_ticks
     *
     * The internal tick timestamp for the scheduler is simply the
     * most recent tick count retrieved. No special arithmetic is
     * applied to it.
     */
    void UpdateLastContextSwitchTime(Thread* thread, Process* process);

    static void OnSwitch(void* this_scheduler);
    void SwitchToCurrent();

    std::shared_ptr<Thread> current_thread = nullptr;
    std::shared_ptr<Thread> selected_thread = nullptr;
    std::shared_ptr<Thread> current_thread_prev = nullptr;
    std::shared_ptr<Thread> selected_thread_set = nullptr;
    std::shared_ptr<Thread> idle_thread = nullptr;

    std::shared_ptr<Common::Fiber> switch_fiber = nullptr;

    Core::System& system;
    u64 last_context_switch_time = 0;
    u64 idle_selection_count = 0;
    const std::size_t core_id;

    Common::SpinLock guard{};

    bool is_context_switch_pending = false;
};

class SchedulerLock {
public:
    explicit SchedulerLock(KernelCore& kernel);
    ~SchedulerLock();

protected:
    KernelCore& kernel;
};

class SchedulerLockAndSleep : public SchedulerLock {
public:
    explicit SchedulerLockAndSleep(KernelCore& kernel, Handle& event_handle, Thread* time_task,
                                   s64 nanoseconds);
    ~SchedulerLockAndSleep();

    void CancelSleep() {
        sleep_cancelled = true;
    }

    void Release();

private:
    Handle& event_handle;
    Thread* time_task;
    s64 nanoseconds;
    bool sleep_cancelled{};
};

} // namespace Kernel
