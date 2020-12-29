// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// This file references various implementation details from Atmosphere, an open-source firmware for
// the Nintendo Switch. Copyright 2018-2020 Atmosphere-NX.

#pragma once

#include <atomic>

#include "common/common_types.h"
#include "common/spin_lock.h"
#include "core/hle/kernel/global_scheduler_context.h"
#include "core/hle/kernel/k_priority_queue.h"
#include "core/hle/kernel/k_scheduler_lock.h"
#include "core/hle/kernel/k_scoped_lock.h"

namespace Common {
class Fiber;
}

namespace Core {
class System;
}

namespace Kernel {

class KernelCore;
class Process;
class SchedulerLock;
class Thread;

class KScheduler final {
public:
    explicit KScheduler(Core::System& system, std::size_t core_id);
    ~KScheduler();

    /// Reschedules to the next available thread (call after current thread is suspended)
    void RescheduleCurrentCore();

    /// Reschedules cores pending reschedule, to be called on EnableScheduling.
    static void RescheduleCores(KernelCore& kernel, u64 cores_pending_reschedule,
                                Core::EmuThreadHandle global_thread);

    /// The next two are for SingleCore Only.
    /// Unload current thread before preempting core.
    void Unload(Thread* thread);

    /// Reload current thread after core preemption.
    void Reload(Thread* thread);

    /// Gets the current running thread
    [[nodiscard]] Thread* GetCurrentThread() const;

    /// Gets the timestamp for the last context switch in ticks.
    [[nodiscard]] u64 GetLastContextSwitchTicks() const;

    [[nodiscard]] bool ContextSwitchPending() const {
        return state.needs_scheduling.load(std::memory_order_relaxed);
    }

    void Initialize();

    void OnThreadStart();

    [[nodiscard]] std::shared_ptr<Common::Fiber>& ControlContext() {
        return switch_fiber;
    }

    [[nodiscard]] const std::shared_ptr<Common::Fiber>& ControlContext() const {
        return switch_fiber;
    }

    [[nodiscard]] u64 UpdateHighestPriorityThread(Thread* highest_thread);

    /**
     * Takes a thread and moves it to the back of the it's priority list.
     *
     * @note This operation can be redundant and no scheduling is changed if marked as so.
     */
    void YieldWithoutCoreMigration();

    /**
     * Takes a thread and moves it to the back of the it's priority list.
     * Afterwards, tries to pick a suggested thread from the suggested queue that has worse time or
     * a better priority than the next thread in the core.
     *
     * @note This operation can be redundant and no scheduling is changed if marked as so.
     */
    void YieldWithCoreMigration();

    /**
     * Takes a thread and moves it out of the scheduling queue.
     * and into the suggested queue. If no thread can be scheduled afterwards in that core,
     * a suggested thread is obtained instead.
     *
     * @note This operation can be redundant and no scheduling is changed if marked as so.
     */
    void YieldToAnyThread();

    /// Notify the scheduler a thread's status has changed.
    static void OnThreadStateChanged(KernelCore& kernel, Thread* thread, ThreadState old_state);

    /// Notify the scheduler a thread's priority has changed.
    static void OnThreadPriorityChanged(KernelCore& kernel, Thread* thread, s32 old_priority);

    /// Notify the scheduler a thread's core and/or affinity mask has changed.
    static void OnThreadAffinityMaskChanged(KernelCore& kernel, Thread* thread,
                                            const KAffinityMask& old_affinity, s32 old_core);

    static bool CanSchedule(KernelCore& kernel);
    static bool IsSchedulerUpdateNeeded(const KernelCore& kernel);
    static void SetSchedulerUpdateNeeded(KernelCore& kernel);
    static void ClearSchedulerUpdateNeeded(KernelCore& kernel);
    static void DisableScheduling(KernelCore& kernel);
    static void EnableScheduling(KernelCore& kernel, u64 cores_needing_scheduling,
                                 Core::EmuThreadHandle global_thread);
    [[nodiscard]] static u64 UpdateHighestPriorityThreads(KernelCore& kernel);

private:
    friend class GlobalSchedulerContext;

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
    [[nodiscard]] static u64 UpdateHighestPriorityThreadsImpl(KernelCore& kernel);

    [[nodiscard]] static KSchedulerPriorityQueue& GetPriorityQueue(KernelCore& kernel);

    void RotateScheduledQueue(s32 core_id, s32 priority);

    void Schedule() {
        ASSERT(GetCurrentThread()->GetDisableDispatchCount() == 1);
        this->ScheduleImpl();
    }

    /// Switches the CPU's active thread context to that of the specified thread
    void ScheduleImpl();

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

    Thread* current_thread{};
    Thread* idle_thread{};

    std::shared_ptr<Common::Fiber> switch_fiber{};

    struct SchedulingState {
        std::atomic<bool> needs_scheduling;
        bool interrupt_task_thread_runnable{};
        bool should_count_idle{};
        u64 idle_count{};
        Thread* highest_priority_thread{};
        void* idle_thread_stack{};
    };

    SchedulingState state;

    Core::System& system;
    u64 last_context_switch_time{};
    const std::size_t core_id;

    Common::SpinLock guard{};
};

class KScopedSchedulerLock : KScopedLock<GlobalSchedulerContext::LockType> {
public:
    explicit KScopedSchedulerLock(KernelCore& kernel);
    ~KScopedSchedulerLock();
};

} // namespace Kernel
