// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <mutex>
#include <vector>
#include "common/common_types.h"
#include "common/thread_queue_list.h"
#include "core/hle/kernel/thread.h"

class ARM_Interface;

namespace Kernel {

class Scheduler final {
public:
    explicit Scheduler(ARM_Interface* cpu_core);
    ~Scheduler();

    /// Returns whether there are any threads that are ready to run.
    bool HaveReadyThreads();

    /// Reschedules to the next available thread (call after current thread is suspended)
    void Reschedule();

    /// Gets the current running thread
    Thread* GetCurrentThread() const;

    /// Adds a new thread to the scheduler
    void AddThread(SharedPtr<Thread> thread, u32 priority);

    /// Removes a thread from the scheduler
    void RemoveThread(Thread* thread);

    /// Schedules a thread that has become "ready"
    void ScheduleThread(Thread* thread, u32 priority);

    /// Unschedules a thread that was already scheduled
    void UnscheduleThread(Thread* thread, u32 priority);

    /// Sets the priority of a thread in the scheduler
    void SetThreadPriority(Thread* thread, u32 priority);

    /// Returns a list of all threads managed by the scheduler
    const std::vector<SharedPtr<Thread>>& GetThreadList() const {
        return thread_list;
    }

private:
    /**
     * Pops and returns the next thread from the thread queue
     * @return A pointer to the next ready thread
     */
    Thread* PopNextReadyThread();

    /**
     * Switches the CPU's active thread context to that of the specified thread
     * @param new_thread The thread to switch to
     */
    void SwitchContext(Thread* new_thread);

    /// Lists all thread ids that aren't deleted/etc.
    std::vector<SharedPtr<Thread>> thread_list;

    /// Lists only ready thread ids.
    Common::ThreadQueueList<Thread*, THREADPRIO_LOWEST + 1> ready_queue;

    SharedPtr<Thread> current_thread = nullptr;

    ARM_Interface* cpu_core;

    static std::mutex scheduler_mutex;
};

} // namespace Kernel
