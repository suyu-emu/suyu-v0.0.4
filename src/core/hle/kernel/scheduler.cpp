// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <set>
#include <unordered_set>
#include <utility>

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/arm/arm_interface.h"
#include "core/core.h"
#include "core/core_cpu.h"
#include "core/core_timing.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/scheduler.h"

namespace Kernel {

void GlobalScheduler::AddThread(SharedPtr<Thread> thread) {
    thread_list.push_back(std::move(thread));
}

void GlobalScheduler::RemoveThread(Thread* thread) {
    thread_list.erase(std::remove(thread_list.begin(), thread_list.end(), thread),
                      thread_list.end());
}

/*
 * SelectThreads, Yield functions originally by TuxSH.
 * licensed under GPLv2 or later under exception provided by the author.
 */

void GlobalScheduler::UnloadThread(s32 core) {
    Scheduler& sched = Core::System::GetInstance().Scheduler(core);
    sched.UnloadThread();
}

void GlobalScheduler::SelectThread(u32 core) {
    auto update_thread = [](Thread* thread, Scheduler& sched) {
        if (thread != sched.selected_thread) {
            if (thread == nullptr) {
                ++sched.idle_selection_count;
            }
            sched.selected_thread = thread;
        }
        sched.context_switch_pending = sched.selected_thread != sched.current_thread;
        std::atomic_thread_fence(std::memory_order_seq_cst);
    };
    Scheduler& sched = Core::System::GetInstance().Scheduler(core);
    Thread* current_thread = nullptr;
    current_thread = scheduled_queue[core].empty() ? nullptr : scheduled_queue[core].front();
    if (!current_thread) {
        Thread* winner = nullptr;
        std::set<s32> sug_cores;
        for (auto thread : suggested_queue[core]) {
            s32 this_core = thread->GetProcessorID();
            Thread* thread_on_core = nullptr;
            if (this_core >= 0) {
                thread_on_core = scheduled_queue[this_core].front();
            }
            if (this_core < 0 || thread != thread_on_core) {
                winner = thread;
                break;
            }
            sug_cores.insert(this_core);
        }
        if (winner && winner->GetPriority() > 2) {
            if (winner->IsRunning()) {
                UnloadThread(winner->GetProcessorID());
            }
            TransferToCore(winner->GetPriority(), core, winner);
            current_thread = winner;
        } else {
            for (auto& src_core : sug_cores) {
                auto it = scheduled_queue[src_core].begin();
                it++;
                if (it != scheduled_queue[src_core].end()) {
                    Thread* thread_on_core = scheduled_queue[src_core].front();
                    Thread* to_change = *it;
                    if (thread_on_core->IsRunning() || to_change->IsRunning()) {
                        UnloadThread(src_core);
                    }
                    TransferToCore(thread_on_core->GetPriority(), core, thread_on_core);
                    current_thread = thread_on_core;
                }
            }
        }
    }
    update_thread(current_thread, sched);
}

void GlobalScheduler::SelectThreads() {
    auto update_thread = [](Thread* thread, Scheduler& sched) {
        if (thread != sched.selected_thread) {
            if (thread == nullptr) {
                ++sched.idle_selection_count;
            }
            sched.selected_thread = thread;
        }
        sched.context_switch_pending = sched.selected_thread != sched.current_thread;
        std::atomic_thread_fence(std::memory_order_seq_cst);
    };

    auto& system = Core::System::GetInstance();

    std::unordered_set<Thread*> picked_threads;
    // This maintain the "current thread is on front of queue" invariant
    std::array<Thread*, NUM_CPU_CORES> current_threads;
    for (u32 i = 0; i < NUM_CPU_CORES; i++) {
        Scheduler& sched = system.Scheduler(i);
        current_threads[i] = scheduled_queue[i].empty() ? nullptr : scheduled_queue[i].front();
        if (current_threads[i])
            picked_threads.insert(current_threads[i]);
        update_thread(current_threads[i], sched);
    }

    // Do some load-balancing. Allow second pass.
    std::array<Thread*, NUM_CPU_CORES> current_threads_2 = current_threads;
    for (u32 i = 0; i < NUM_CPU_CORES; i++) {
        if (!scheduled_queue[i].empty()) {
            continue;
        }
        Thread* winner = nullptr;
        for (auto thread : suggested_queue[i]) {
            if (thread->GetProcessorID() < 0 || thread != current_threads[i]) {
                if (picked_threads.count(thread) == 0 && !thread->IsRunning()) {
                    winner = thread;
                    break;
                }
            }
        }
        if (winner) {
            TransferToCore(winner->GetPriority(), i, winner);
            current_threads_2[i] = winner;
            picked_threads.insert(winner);
        }
    }

    // See which to-be-current threads have changed & update accordingly
    for (u32 i = 0; i < NUM_CPU_CORES; i++) {
        Scheduler& sched = system.Scheduler(i);
        if (current_threads_2[i] != current_threads[i]) {
            update_thread(current_threads_2[i], sched);
        }
    }

    reselection_pending.store(false, std::memory_order_release);
}

void GlobalScheduler::YieldThread(Thread* yielding_thread) {
    // Note: caller should use critical section, etc.
    u32 core_id = static_cast<u32>(yielding_thread->GetProcessorID());
    u32 priority = yielding_thread->GetPriority();

    // Yield the thread
    ASSERT_MSG(yielding_thread == scheduled_queue[core_id].front(priority),
               "Thread yielding without being in front");
    scheduled_queue[core_id].yield(priority);

    Thread* winner = scheduled_queue[core_id].front(priority);
    AskForReselectionOrMarkRedundant(yielding_thread, winner);
}

void GlobalScheduler::YieldThreadAndBalanceLoad(Thread* yielding_thread) {
    // Note: caller should check if !thread.IsSchedulerOperationRedundant and use critical section,
    // etc.
    u32 core_id = static_cast<u32>(yielding_thread->GetProcessorID());
    u32 priority = yielding_thread->GetPriority();

    // Yield the thread
    ASSERT_MSG(yielding_thread == scheduled_queue[core_id].front(priority),
               "Thread yielding without being in front");
    scheduled_queue[core_id].yield(priority);

    std::array<Thread*, NUM_CPU_CORES> current_threads;
    for (u32 i = 0; i < NUM_CPU_CORES; i++) {
        current_threads[i] = scheduled_queue[i].empty() ? nullptr : scheduled_queue[i].front();
    }

    Thread* next_thread = scheduled_queue[core_id].front(priority);
    Thread* winner = nullptr;
    for (auto& thread : suggested_queue[core_id]) {
        s32 source_core = thread->GetProcessorID();
        if (source_core >= 0) {
            if (current_threads[source_core] != nullptr) {
                if (thread == current_threads[source_core] ||
                    current_threads[source_core]->GetPriority() < min_regular_priority)
                    continue;
            }
            if (next_thread->GetLastRunningTicks() >= thread->GetLastRunningTicks() ||
                next_thread->GetPriority() < thread->GetPriority()) {
                if (thread->GetPriority() <= priority) {
                    winner = thread;
                    break;
                }
            }
        }
    }

    if (winner != nullptr) {
        if (winner != yielding_thread) {
            if (winner->IsRunning())
                UnloadThread(winner->GetProcessorID());
            TransferToCore(winner->GetPriority(), core_id, winner);
        }
    } else {
        winner = next_thread;
    }

    AskForReselectionOrMarkRedundant(yielding_thread, winner);
}

void GlobalScheduler::YieldThreadAndWaitForLoadBalancing(Thread* yielding_thread) {
    // Note: caller should check if !thread.IsSchedulerOperationRedundant and use critical section,
    // etc.
    Thread* winner = nullptr;
    u32 core_id = static_cast<u32>(yielding_thread->GetProcessorID());

    // Remove the thread from its scheduled mlq, put it on the corresponding "suggested" one instead
    TransferToCore(yielding_thread->GetPriority(), -1, yielding_thread);

    // If the core is idle, perform load balancing, excluding the threads that have just used this
    // function...
    if (scheduled_queue[core_id].empty()) {
        // Here, "current_threads" is calculated after the ""yield"", unlike yield -1
        std::array<Thread*, NUM_CPU_CORES> current_threads;
        for (u32 i = 0; i < NUM_CPU_CORES; i++) {
            current_threads[i] = scheduled_queue[i].empty() ? nullptr : scheduled_queue[i].front();
        }
        for (auto& thread : suggested_queue[core_id]) {
            s32 source_core = thread->GetProcessorID();
            if (source_core < 0 || thread == current_threads[source_core])
                continue;
            if (current_threads[source_core] == nullptr ||
                current_threads[source_core]->GetPriority() >= min_regular_priority) {
                winner = thread;
            }
            break;
        }
        if (winner != nullptr) {
            if (winner != yielding_thread) {
                if (winner->IsRunning())
                    UnloadThread(winner->GetProcessorID());
                TransferToCore(winner->GetPriority(), core_id, winner);
            }
        } else {
            winner = yielding_thread;
        }
    }

    AskForReselectionOrMarkRedundant(yielding_thread, winner);
}

void GlobalScheduler::AskForReselectionOrMarkRedundant(Thread* current_thread, Thread* winner) {
    if (current_thread == winner) {
        // Nintendo (not us) has a nullderef bug on current_thread->owner, but which is never
        // triggered.
        // current_thread->SetRedundantSchedulerOperation();
    } else {
        reselection_pending.store(true, std::memory_order_release);
    }
}

GlobalScheduler::~GlobalScheduler() = default;

Scheduler::Scheduler(Core::System& system, Core::ARM_Interface& cpu_core, u32 id)
    : system(system), cpu_core(cpu_core), id(id) {}

Scheduler::~Scheduler() {}

bool Scheduler::HaveReadyThreads() const {
    return system.GlobalScheduler().HaveReadyThreads(id);
}

Thread* Scheduler::GetCurrentThread() const {
    return current_thread.get();
}

Thread* Scheduler::GetSelectedThread() const {
    return selected_thread.get();
}

void Scheduler::SelectThreads() {
    system.GlobalScheduler().SelectThread(id);
}

u64 Scheduler::GetLastContextSwitchTicks() const {
    return last_context_switch_time;
}

void Scheduler::TryDoContextSwitch() {
    if (context_switch_pending)
        SwitchContext();
}

void Scheduler::UnloadThread() {
    Thread* const previous_thread = GetCurrentThread();
    Process* const previous_process = Core::CurrentProcess();

    UpdateLastContextSwitchTime(previous_thread, previous_process);

    // Save context for previous thread
    if (previous_thread) {
        cpu_core.SaveContext(previous_thread->GetContext());
        // Save the TPIDR_EL0 system register in case it was modified.
        previous_thread->SetTPIDR_EL0(cpu_core.GetTPIDR_EL0());

        if (previous_thread->GetStatus() == ThreadStatus::Running) {
            // This is only the case when a reschedule is triggered without the current thread
            // yielding execution (i.e. an event triggered, system core time-sliced, etc)
            previous_thread->SetStatus(ThreadStatus::Ready);
        }
        previous_thread->SetIsRunning(false);
    }
    current_thread = nullptr;
}

void Scheduler::SwitchContext() {
    Thread* const previous_thread = GetCurrentThread();
    Thread* const new_thread = GetSelectedThread();

    context_switch_pending = false;
    if (new_thread == previous_thread)
        return;

    Process* const previous_process = Core::CurrentProcess();

    UpdateLastContextSwitchTime(previous_thread, previous_process);

    // Save context for previous thread
    if (previous_thread) {
        cpu_core.SaveContext(previous_thread->GetContext());
        // Save the TPIDR_EL0 system register in case it was modified.
        previous_thread->SetTPIDR_EL0(cpu_core.GetTPIDR_EL0());

        if (previous_thread->GetStatus() == ThreadStatus::Running) {
            // This is only the case when a reschedule is triggered without the current thread
            // yielding execution (i.e. an event triggered, system core time-sliced, etc)
            previous_thread->SetStatus(ThreadStatus::Ready);
        }
        previous_thread->SetIsRunning(false);
    }

    // Load context of new thread
    if (new_thread) {
        ASSERT_MSG(new_thread->GetProcessorID() == this->id,
                   "Thread must be assigned to this core.");
        ASSERT_MSG(new_thread->GetStatus() == ThreadStatus::Ready,
                   "Thread must be ready to become running.");

        // Cancel any outstanding wakeup events for this thread
        new_thread->CancelWakeupTimer();
        current_thread = new_thread;
        new_thread->SetStatus(ThreadStatus::Running);
        new_thread->SetIsRunning(true);

        auto* const thread_owner_process = current_thread->GetOwnerProcess();
        if (previous_process != thread_owner_process) {
            system.Kernel().MakeCurrentProcess(thread_owner_process);
        }

        cpu_core.LoadContext(new_thread->GetContext());
        cpu_core.SetTlsAddress(new_thread->GetTLSAddress());
        cpu_core.SetTPIDR_EL0(new_thread->GetTPIDR_EL0());
        cpu_core.ClearExclusiveState();
    } else {
        current_thread = nullptr;
        // Note: We do not reset the current process and current page table when idling because
        // technically we haven't changed processes, our threads are just paused.
    }
}

void Scheduler::UpdateLastContextSwitchTime(Thread* thread, Process* process) {
    const u64 prev_switch_ticks = last_context_switch_time;
    const u64 most_recent_switch_ticks = Core::System::GetInstance().CoreTiming().GetTicks();
    const u64 update_ticks = most_recent_switch_ticks - prev_switch_ticks;

    if (thread != nullptr) {
        thread->UpdateCPUTimeTicks(update_ticks);
    }

    if (process != nullptr) {
        process->UpdateCPUTimeTicks(update_ticks);
    }

    last_context_switch_time = most_recent_switch_ticks;
}

} // namespace Kernel
