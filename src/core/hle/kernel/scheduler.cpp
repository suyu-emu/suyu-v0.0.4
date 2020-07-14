// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// SelectThreads, Yield functions originally by TuxSH.
// licensed under GPLv2 or later under exception provided by the author.

#include <algorithm>
#include <mutex>
#include <set>
#include <unordered_set>
#include <utility>

#include "common/assert.h"
#include "common/bit_util.h"
#include "common/fiber.h"
#include "common/logging/log.h"
#include "core/arm/arm_interface.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/cpu_manager.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/physical_core.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/scheduler.h"
#include "core/hle/kernel/time_manager.h"

namespace Kernel {

GlobalScheduler::GlobalScheduler(KernelCore& kernel) : kernel{kernel} {}

GlobalScheduler::~GlobalScheduler() = default;

void GlobalScheduler::AddThread(std::shared_ptr<Thread> thread) {
    std::scoped_lock lock{global_list_guard};
    thread_list.push_back(std::move(thread));
}

void GlobalScheduler::RemoveThread(std::shared_ptr<Thread> thread) {
    std::scoped_lock lock{global_list_guard};
    thread_list.erase(std::remove(thread_list.begin(), thread_list.end(), thread),
                      thread_list.end());
}

u32 GlobalScheduler::SelectThreads() {
    ASSERT(is_locked);
    const auto update_thread = [](Thread* thread, Scheduler& sched) {
        std::scoped_lock lock{sched.guard};
        if (thread != sched.selected_thread_set.get()) {
            if (thread == nullptr) {
                ++sched.idle_selection_count;
            }
            sched.selected_thread_set = SharedFrom(thread);
        }
        const bool reschedule_pending =
            sched.is_context_switch_pending || (sched.selected_thread_set != sched.current_thread);
        sched.is_context_switch_pending = reschedule_pending;
        std::atomic_thread_fence(std::memory_order_seq_cst);
        return reschedule_pending;
    };
    if (!is_reselection_pending.load()) {
        return 0;
    }
    std::array<Thread*, Core::Hardware::NUM_CPU_CORES> top_threads{};

    u32 idle_cores{};

    // Step 1: Get top thread in schedule queue.
    for (u32 core = 0; core < Core::Hardware::NUM_CPU_CORES; core++) {
        Thread* top_thread =
            scheduled_queue[core].empty() ? nullptr : scheduled_queue[core].front();
        if (top_thread != nullptr) {
            // TODO(Blinkhawk): Implement Thread Pinning
        } else {
            idle_cores |= (1ul << core);
        }
        top_threads[core] = top_thread;
    }

    while (idle_cores != 0) {
        u32 core_id = Common::CountTrailingZeroes32(idle_cores);

        if (!suggested_queue[core_id].empty()) {
            std::array<s32, Core::Hardware::NUM_CPU_CORES> migration_candidates{};
            std::size_t num_candidates = 0;
            auto iter = suggested_queue[core_id].begin();
            Thread* suggested = nullptr;
            // Step 2: Try selecting a suggested thread.
            while (iter != suggested_queue[core_id].end()) {
                suggested = *iter;
                iter++;
                s32 suggested_core_id = suggested->GetProcessorID();
                Thread* top_thread =
                    suggested_core_id >= 0 ? top_threads[suggested_core_id] : nullptr;
                if (top_thread != suggested) {
                    if (top_thread != nullptr &&
                        top_thread->GetPriority() < THREADPRIO_MAX_CORE_MIGRATION) {
                        suggested = nullptr;
                        break;
                        // There's a too high thread to do core migration, cancel
                    }
                    TransferToCore(suggested->GetPriority(), static_cast<s32>(core_id), suggested);
                    break;
                }
                suggested = nullptr;
                migration_candidates[num_candidates++] = suggested_core_id;
            }
            // Step 3: Select a suggested thread from another core
            if (suggested == nullptr) {
                for (std::size_t i = 0; i < num_candidates; i++) {
                    s32 candidate_core = migration_candidates[i];
                    suggested = top_threads[candidate_core];
                    auto it = scheduled_queue[candidate_core].begin();
                    it++;
                    Thread* next = it != scheduled_queue[candidate_core].end() ? *it : nullptr;
                    if (next != nullptr) {
                        TransferToCore(suggested->GetPriority(), static_cast<s32>(core_id),
                                       suggested);
                        top_threads[candidate_core] = next;
                        break;
                    } else {
                        suggested = nullptr;
                    }
                }
            }
            top_threads[core_id] = suggested;
        }

        idle_cores &= ~(1ul << core_id);
    }
    u32 cores_needing_context_switch{};
    for (u32 core = 0; core < Core::Hardware::NUM_CPU_CORES; core++) {
        Scheduler& sched = kernel.Scheduler(core);
        ASSERT(top_threads[core] == nullptr || top_threads[core]->GetProcessorID() == core);
        if (update_thread(top_threads[core], sched)) {
            cores_needing_context_switch |= (1ul << core);
        }
    }
    return cores_needing_context_switch;
}

bool GlobalScheduler::YieldThread(Thread* yielding_thread) {
    ASSERT(is_locked);
    // Note: caller should use critical section, etc.
    if (!yielding_thread->IsRunnable()) {
        // Normally this case shouldn't happen except for SetThreadActivity.
        is_reselection_pending.store(true, std::memory_order_release);
        return false;
    }
    const u32 core_id = static_cast<u32>(yielding_thread->GetProcessorID());
    const u32 priority = yielding_thread->GetPriority();

    // Yield the thread
    Reschedule(priority, core_id, yielding_thread);
    const Thread* const winner = scheduled_queue[core_id].front();
    if (kernel.GetCurrentHostThreadID() != core_id) {
        is_reselection_pending.store(true, std::memory_order_release);
    }

    return AskForReselectionOrMarkRedundant(yielding_thread, winner);
}

bool GlobalScheduler::YieldThreadAndBalanceLoad(Thread* yielding_thread) {
    ASSERT(is_locked);
    // Note: caller should check if !thread.IsSchedulerOperationRedundant and use critical section,
    // etc.
    if (!yielding_thread->IsRunnable()) {
        // Normally this case shouldn't happen except for SetThreadActivity.
        is_reselection_pending.store(true, std::memory_order_release);
        return false;
    }
    const u32 core_id = static_cast<u32>(yielding_thread->GetProcessorID());
    const u32 priority = yielding_thread->GetPriority();

    // Yield the thread
    Reschedule(priority, core_id, yielding_thread);

    std::array<Thread*, Core::Hardware::NUM_CPU_CORES> current_threads;
    for (std::size_t i = 0; i < current_threads.size(); i++) {
        current_threads[i] = scheduled_queue[i].empty() ? nullptr : scheduled_queue[i].front();
    }

    Thread* next_thread = scheduled_queue[core_id].front(priority);
    Thread* winner = nullptr;
    for (auto& thread : suggested_queue[core_id]) {
        const s32 source_core = thread->GetProcessorID();
        if (source_core >= 0) {
            if (current_threads[source_core] != nullptr) {
                if (thread == current_threads[source_core] ||
                    current_threads[source_core]->GetPriority() < min_regular_priority) {
                    continue;
                }
            }
        }
        if (next_thread->GetLastRunningTicks() >= thread->GetLastRunningTicks() ||
            next_thread->GetPriority() < thread->GetPriority()) {
            if (thread->GetPriority() <= priority) {
                winner = thread;
                break;
            }
        }
    }

    if (winner != nullptr) {
        if (winner != yielding_thread) {
            TransferToCore(winner->GetPriority(), s32(core_id), winner);
        }
    } else {
        winner = next_thread;
    }

    if (kernel.GetCurrentHostThreadID() != core_id) {
        is_reselection_pending.store(true, std::memory_order_release);
    }

    return AskForReselectionOrMarkRedundant(yielding_thread, winner);
}

bool GlobalScheduler::YieldThreadAndWaitForLoadBalancing(Thread* yielding_thread) {
    ASSERT(is_locked);
    // Note: caller should check if !thread.IsSchedulerOperationRedundant and use critical section,
    // etc.
    if (!yielding_thread->IsRunnable()) {
        // Normally this case shouldn't happen except for SetThreadActivity.
        is_reselection_pending.store(true, std::memory_order_release);
        return false;
    }
    Thread* winner = nullptr;
    const u32 core_id = static_cast<u32>(yielding_thread->GetProcessorID());

    // Remove the thread from its scheduled mlq, put it on the corresponding "suggested" one instead
    TransferToCore(yielding_thread->GetPriority(), -1, yielding_thread);

    // If the core is idle, perform load balancing, excluding the threads that have just used this
    // function...
    if (scheduled_queue[core_id].empty()) {
        // Here, "current_threads" is calculated after the ""yield"", unlike yield -1
        std::array<Thread*, Core::Hardware::NUM_CPU_CORES> current_threads;
        for (std::size_t i = 0; i < current_threads.size(); i++) {
            current_threads[i] = scheduled_queue[i].empty() ? nullptr : scheduled_queue[i].front();
        }
        for (auto& thread : suggested_queue[core_id]) {
            const s32 source_core = thread->GetProcessorID();
            if (source_core < 0 || thread == current_threads[source_core]) {
                continue;
            }
            if (current_threads[source_core] == nullptr ||
                current_threads[source_core]->GetPriority() >= min_regular_priority) {
                winner = thread;
            }
            break;
        }
        if (winner != nullptr) {
            if (winner != yielding_thread) {
                TransferToCore(winner->GetPriority(), static_cast<s32>(core_id), winner);
            }
        } else {
            winner = yielding_thread;
        }
    } else {
        winner = scheduled_queue[core_id].front();
    }

    if (kernel.GetCurrentHostThreadID() != core_id) {
        is_reselection_pending.store(true, std::memory_order_release);
    }

    return AskForReselectionOrMarkRedundant(yielding_thread, winner);
}

void GlobalScheduler::PreemptThreads() {
    ASSERT(is_locked);
    for (std::size_t core_id = 0; core_id < Core::Hardware::NUM_CPU_CORES; core_id++) {
        const u32 priority = preemption_priorities[core_id];

        if (scheduled_queue[core_id].size(priority) > 0) {
            if (scheduled_queue[core_id].size(priority) > 1) {
                scheduled_queue[core_id].front(priority)->IncrementYieldCount();
            }
            scheduled_queue[core_id].yield(priority);
            if (scheduled_queue[core_id].size(priority) > 1) {
                scheduled_queue[core_id].front(priority)->IncrementYieldCount();
            }
        }

        Thread* current_thread =
            scheduled_queue[core_id].empty() ? nullptr : scheduled_queue[core_id].front();
        Thread* winner = nullptr;
        for (auto& thread : suggested_queue[core_id]) {
            const s32 source_core = thread->GetProcessorID();
            if (thread->GetPriority() != priority) {
                continue;
            }
            if (source_core >= 0) {
                Thread* next_thread = scheduled_queue[source_core].empty()
                                          ? nullptr
                                          : scheduled_queue[source_core].front();
                if (next_thread != nullptr && next_thread->GetPriority() < 2) {
                    break;
                }
                if (next_thread == thread) {
                    continue;
                }
            }
            if (current_thread != nullptr &&
                current_thread->GetLastRunningTicks() >= thread->GetLastRunningTicks()) {
                winner = thread;
                break;
            }
        }

        if (winner != nullptr) {
            TransferToCore(winner->GetPriority(), s32(core_id), winner);
            current_thread =
                winner->GetPriority() <= current_thread->GetPriority() ? winner : current_thread;
        }

        if (current_thread != nullptr && current_thread->GetPriority() > priority) {
            for (auto& thread : suggested_queue[core_id]) {
                const s32 source_core = thread->GetProcessorID();
                if (thread->GetPriority() < priority) {
                    continue;
                }
                if (source_core >= 0) {
                    Thread* next_thread = scheduled_queue[source_core].empty()
                                              ? nullptr
                                              : scheduled_queue[source_core].front();
                    if (next_thread != nullptr && next_thread->GetPriority() < 2) {
                        break;
                    }
                    if (next_thread == thread) {
                        continue;
                    }
                }
                if (current_thread != nullptr &&
                    current_thread->GetLastRunningTicks() >= thread->GetLastRunningTicks()) {
                    winner = thread;
                    break;
                }
            }

            if (winner != nullptr) {
                TransferToCore(winner->GetPriority(), s32(core_id), winner);
                current_thread = winner;
            }
        }

        is_reselection_pending.store(true, std::memory_order_release);
    }
}

void GlobalScheduler::EnableInterruptAndSchedule(u32 cores_pending_reschedule,
                                                 Core::EmuThreadHandle global_thread) {
    u32 current_core = global_thread.host_handle;
    bool must_context_switch = global_thread.guest_handle != InvalidHandle &&
                               (current_core < Core::Hardware::NUM_CPU_CORES);
    while (cores_pending_reschedule != 0) {
        u32 core = Common::CountTrailingZeroes32(cores_pending_reschedule);
        ASSERT(core < Core::Hardware::NUM_CPU_CORES);
        if (!must_context_switch || core != current_core) {
            auto& phys_core = kernel.PhysicalCore(core);
            phys_core.Interrupt();
        } else {
            must_context_switch = true;
        }
        cores_pending_reschedule &= ~(1ul << core);
    }
    if (must_context_switch) {
        auto& core_scheduler = kernel.CurrentScheduler();
        kernel.ExitSVCProfile();
        core_scheduler.TryDoContextSwitch();
        kernel.EnterSVCProfile();
    }
}

void GlobalScheduler::Suggest(u32 priority, std::size_t core, Thread* thread) {
    ASSERT(is_locked);
    suggested_queue[core].add(thread, priority);
}

void GlobalScheduler::Unsuggest(u32 priority, std::size_t core, Thread* thread) {
    ASSERT(is_locked);
    suggested_queue[core].remove(thread, priority);
}

void GlobalScheduler::Schedule(u32 priority, std::size_t core, Thread* thread) {
    ASSERT(is_locked);
    ASSERT_MSG(thread->GetProcessorID() == s32(core), "Thread must be assigned to this core.");
    scheduled_queue[core].add(thread, priority);
}

void GlobalScheduler::SchedulePrepend(u32 priority, std::size_t core, Thread* thread) {
    ASSERT(is_locked);
    ASSERT_MSG(thread->GetProcessorID() == s32(core), "Thread must be assigned to this core.");
    scheduled_queue[core].add(thread, priority, false);
}

void GlobalScheduler::Reschedule(u32 priority, std::size_t core, Thread* thread) {
    ASSERT(is_locked);
    scheduled_queue[core].remove(thread, priority);
    scheduled_queue[core].add(thread, priority);
}

void GlobalScheduler::Unschedule(u32 priority, std::size_t core, Thread* thread) {
    ASSERT(is_locked);
    scheduled_queue[core].remove(thread, priority);
}

void GlobalScheduler::TransferToCore(u32 priority, s32 destination_core, Thread* thread) {
    ASSERT(is_locked);
    const bool schedulable = thread->GetPriority() < THREADPRIO_COUNT;
    const s32 source_core = thread->GetProcessorID();
    if (source_core == destination_core || !schedulable) {
        return;
    }
    thread->SetProcessorID(destination_core);
    if (source_core >= 0) {
        Unschedule(priority, static_cast<u32>(source_core), thread);
    }
    if (destination_core >= 0) {
        Unsuggest(priority, static_cast<u32>(destination_core), thread);
        Schedule(priority, static_cast<u32>(destination_core), thread);
    }
    if (source_core >= 0) {
        Suggest(priority, static_cast<u32>(source_core), thread);
    }
}

bool GlobalScheduler::AskForReselectionOrMarkRedundant(Thread* current_thread,
                                                       const Thread* winner) {
    if (current_thread == winner) {
        current_thread->IncrementYieldCount();
        return true;
    } else {
        is_reselection_pending.store(true, std::memory_order_release);
        return false;
    }
}

void GlobalScheduler::AdjustSchedulingOnStatus(Thread* thread, u32 old_flags) {
    if (old_flags == thread->scheduling_state) {
        return;
    }
    ASSERT(is_locked);

    if (old_flags == static_cast<u32>(ThreadSchedStatus::Runnable)) {
        // In this case the thread was running, now it's pausing/exitting
        if (thread->processor_id >= 0) {
            Unschedule(thread->current_priority, static_cast<u32>(thread->processor_id), thread);
        }

        for (u32 core = 0; core < Core::Hardware::NUM_CPU_CORES; core++) {
            if (core != static_cast<u32>(thread->processor_id) &&
                ((thread->affinity_mask >> core) & 1) != 0) {
                Unsuggest(thread->current_priority, core, thread);
            }
        }
    } else if (thread->scheduling_state == static_cast<u32>(ThreadSchedStatus::Runnable)) {
        // The thread is now set to running from being stopped
        if (thread->processor_id >= 0) {
            Schedule(thread->current_priority, static_cast<u32>(thread->processor_id), thread);
        }

        for (u32 core = 0; core < Core::Hardware::NUM_CPU_CORES; core++) {
            if (core != static_cast<u32>(thread->processor_id) &&
                ((thread->affinity_mask >> core) & 1) != 0) {
                Suggest(thread->current_priority, core, thread);
            }
        }
    }

    SetReselectionPending();
}

void GlobalScheduler::AdjustSchedulingOnPriority(Thread* thread, u32 old_priority) {
    if (thread->scheduling_state != static_cast<u32>(ThreadSchedStatus::Runnable)) {
        return;
    }
    ASSERT(is_locked);
    if (thread->processor_id >= 0) {
        Unschedule(old_priority, static_cast<u32>(thread->processor_id), thread);
    }

    for (u32 core = 0; core < Core::Hardware::NUM_CPU_CORES; core++) {
        if (core != static_cast<u32>(thread->processor_id) &&
            ((thread->affinity_mask >> core) & 1) != 0) {
            Unsuggest(old_priority, core, thread);
        }
    }

    if (thread->processor_id >= 0) {
        if (thread == kernel.CurrentScheduler().GetCurrentThread()) {
            SchedulePrepend(thread->current_priority, static_cast<u32>(thread->processor_id),
                            thread);
        } else {
            Schedule(thread->current_priority, static_cast<u32>(thread->processor_id), thread);
        }
    }

    for (u32 core = 0; core < Core::Hardware::NUM_CPU_CORES; core++) {
        if (core != static_cast<u32>(thread->processor_id) &&
            ((thread->affinity_mask >> core) & 1) != 0) {
            Suggest(thread->current_priority, core, thread);
        }
    }
    thread->IncrementYieldCount();
    SetReselectionPending();
}

void GlobalScheduler::AdjustSchedulingOnAffinity(Thread* thread, u64 old_affinity_mask,
                                                 s32 old_core) {
    if (thread->scheduling_state != static_cast<u32>(ThreadSchedStatus::Runnable) ||
        thread->current_priority >= THREADPRIO_COUNT) {
        return;
    }
    ASSERT(is_locked);

    for (u32 core = 0; core < Core::Hardware::NUM_CPU_CORES; core++) {
        if (((old_affinity_mask >> core) & 1) != 0) {
            if (core == static_cast<u32>(old_core)) {
                Unschedule(thread->current_priority, core, thread);
            } else {
                Unsuggest(thread->current_priority, core, thread);
            }
        }
    }

    for (u32 core = 0; core < Core::Hardware::NUM_CPU_CORES; core++) {
        if (((thread->affinity_mask >> core) & 1) != 0) {
            if (core == static_cast<u32>(thread->processor_id)) {
                Schedule(thread->current_priority, core, thread);
            } else {
                Suggest(thread->current_priority, core, thread);
            }
        }
    }

    thread->IncrementYieldCount();
    SetReselectionPending();
}

void GlobalScheduler::Shutdown() {
    for (std::size_t core = 0; core < Core::Hardware::NUM_CPU_CORES; core++) {
        scheduled_queue[core].clear();
        suggested_queue[core].clear();
    }
    thread_list.clear();
}

void GlobalScheduler::Lock() {
    Core::EmuThreadHandle current_thread = kernel.GetCurrentEmuThreadID();
    ASSERT(!current_thread.IsInvalid());
    if (current_thread == current_owner) {
        ++scope_lock;
    } else {
        inner_lock.lock();
        is_locked = true;
        current_owner = current_thread;
        ASSERT(current_owner != Core::EmuThreadHandle::InvalidHandle());
        scope_lock = 1;
    }
}

void GlobalScheduler::Unlock() {
    if (--scope_lock != 0) {
        ASSERT(scope_lock > 0);
        return;
    }
    u32 cores_pending_reschedule = SelectThreads();
    Core::EmuThreadHandle leaving_thread = current_owner;
    current_owner = Core::EmuThreadHandle::InvalidHandle();
    scope_lock = 1;
    is_locked = false;
    inner_lock.unlock();
    EnableInterruptAndSchedule(cores_pending_reschedule, leaving_thread);
}

Scheduler::Scheduler(Core::System& system, std::size_t core_id) : system(system), core_id(core_id) {
    switch_fiber = std::make_shared<Common::Fiber>(std::function<void(void*)>(OnSwitch), this);
}

Scheduler::~Scheduler() = default;

bool Scheduler::HaveReadyThreads() const {
    return system.GlobalScheduler().HaveReadyThreads(core_id);
}

Thread* Scheduler::GetCurrentThread() const {
    if (current_thread) {
        return current_thread.get();
    }
    return idle_thread.get();
}

Thread* Scheduler::GetSelectedThread() const {
    return selected_thread.get();
}

u64 Scheduler::GetLastContextSwitchTicks() const {
    return last_context_switch_time;
}

void Scheduler::TryDoContextSwitch() {
    auto& phys_core = system.Kernel().CurrentPhysicalCore();
    if (phys_core.IsInterrupted()) {
        phys_core.ClearInterrupt();
    }
    guard.lock();
    if (is_context_switch_pending) {
        SwitchContext();
    } else {
        guard.unlock();
    }
}

void Scheduler::OnThreadStart() {
    SwitchContextStep2();
}

void Scheduler::Unload() {
    Thread* thread = current_thread.get();
    if (thread) {
        thread->SetContinuousOnSVC(false);
        thread->last_running_ticks = system.CoreTiming().GetCPUTicks();
        thread->SetIsRunning(false);
        if (!thread->IsHLEThread() && !thread->HasExited()) {
            Core::ARM_Interface& cpu_core = thread->ArmInterface();
            cpu_core.SaveContext(thread->GetContext32());
            cpu_core.SaveContext(thread->GetContext64());
            // Save the TPIDR_EL0 system register in case it was modified.
            thread->SetTPIDR_EL0(cpu_core.GetTPIDR_EL0());
            cpu_core.ClearExclusiveState();
        }
        thread->context_guard.unlock();
    }
}

void Scheduler::Reload() {
    Thread* thread = current_thread.get();
    if (thread) {
        ASSERT_MSG(thread->GetSchedulingStatus() == ThreadSchedStatus::Runnable,
                   "Thread must be runnable.");

        // Cancel any outstanding wakeup events for this thread
        thread->SetIsRunning(true);
        thread->SetWasRunning(false);
        thread->last_running_ticks = system.CoreTiming().GetCPUTicks();

        auto* const thread_owner_process = thread->GetOwnerProcess();
        if (thread_owner_process != nullptr) {
            system.Kernel().MakeCurrentProcess(thread_owner_process);
        }
        if (!thread->IsHLEThread()) {
            Core::ARM_Interface& cpu_core = thread->ArmInterface();
            cpu_core.LoadContext(thread->GetContext32());
            cpu_core.LoadContext(thread->GetContext64());
            cpu_core.SetTlsAddress(thread->GetTLSAddress());
            cpu_core.SetTPIDR_EL0(thread->GetTPIDR_EL0());
            cpu_core.ChangeProcessorID(this->core_id);
            cpu_core.ClearExclusiveState();
        }
    }
}

void Scheduler::SwitchContextStep2() {
    Thread* previous_thread = current_thread_prev.get();
    Thread* new_thread = selected_thread.get();

    // Load context of new thread
    Process* const previous_process =
        previous_thread != nullptr ? previous_thread->GetOwnerProcess() : nullptr;

    if (new_thread) {
        ASSERT_MSG(new_thread->GetSchedulingStatus() == ThreadSchedStatus::Runnable,
                   "Thread must be runnable.");

        // Cancel any outstanding wakeup events for this thread
        new_thread->SetIsRunning(true);
        new_thread->last_running_ticks = system.CoreTiming().GetCPUTicks();
        new_thread->SetWasRunning(false);

        auto* const thread_owner_process = current_thread->GetOwnerProcess();
        if (thread_owner_process != nullptr) {
            system.Kernel().MakeCurrentProcess(thread_owner_process);
        }
        if (!new_thread->IsHLEThread()) {
            Core::ARM_Interface& cpu_core = new_thread->ArmInterface();
            cpu_core.LoadContext(new_thread->GetContext32());
            cpu_core.LoadContext(new_thread->GetContext64());
            cpu_core.SetTlsAddress(new_thread->GetTLSAddress());
            cpu_core.SetTPIDR_EL0(new_thread->GetTPIDR_EL0());
            cpu_core.ChangeProcessorID(this->core_id);
            cpu_core.ClearExclusiveState();
        }
    }

    TryDoContextSwitch();
}

void Scheduler::SwitchContext() {
    current_thread_prev = current_thread;
    selected_thread = selected_thread_set;
    Thread* previous_thread = current_thread_prev.get();
    Thread* new_thread = selected_thread.get();
    current_thread = selected_thread;

    is_context_switch_pending = false;

    if (new_thread == previous_thread) {
        guard.unlock();
        return;
    }

    Process* const previous_process = system.Kernel().CurrentProcess();

    UpdateLastContextSwitchTime(previous_thread, previous_process);

    // Save context for previous thread
    if (previous_thread) {
        if (new_thread != nullptr && new_thread->IsSuspendThread()) {
            previous_thread->SetWasRunning(true);
        }
        previous_thread->SetContinuousOnSVC(false);
        previous_thread->last_running_ticks = system.CoreTiming().GetCPUTicks();
        previous_thread->SetIsRunning(false);
        if (!previous_thread->IsHLEThread() && !previous_thread->HasExited()) {
            Core::ARM_Interface& cpu_core = previous_thread->ArmInterface();
            cpu_core.SaveContext(previous_thread->GetContext32());
            cpu_core.SaveContext(previous_thread->GetContext64());
            // Save the TPIDR_EL0 system register in case it was modified.
            previous_thread->SetTPIDR_EL0(cpu_core.GetTPIDR_EL0());
            cpu_core.ClearExclusiveState();
        }
        previous_thread->context_guard.unlock();
    }

    std::shared_ptr<Common::Fiber>* old_context;
    if (previous_thread != nullptr) {
        old_context = &previous_thread->GetHostContext();
    } else {
        old_context = &idle_thread->GetHostContext();
    }
    guard.unlock();

    Common::Fiber::YieldTo(*old_context, switch_fiber);
    /// When a thread wakes up, the scheduler may have changed to other in another core.
    auto& next_scheduler = system.Kernel().CurrentScheduler();
    next_scheduler.SwitchContextStep2();
}

void Scheduler::OnSwitch(void* this_scheduler) {
    Scheduler* sched = static_cast<Scheduler*>(this_scheduler);
    sched->SwitchToCurrent();
}

void Scheduler::SwitchToCurrent() {
    while (true) {
        {
            std::scoped_lock lock{guard};
            selected_thread = selected_thread_set;
            current_thread = selected_thread;
            is_context_switch_pending = false;
        }
        while (!is_context_switch_pending) {
            if (current_thread != nullptr && !current_thread->IsHLEThread()) {
                current_thread->context_guard.lock();
                if (!current_thread->IsRunnable()) {
                    current_thread->context_guard.unlock();
                    break;
                }
                if (current_thread->GetProcessorID() != core_id) {
                    current_thread->context_guard.unlock();
                    break;
                }
            }
            std::shared_ptr<Common::Fiber>* next_context;
            if (current_thread != nullptr) {
                next_context = &current_thread->GetHostContext();
            } else {
                next_context = &idle_thread->GetHostContext();
            }
            Common::Fiber::YieldTo(switch_fiber, *next_context);
        }
    }
}

void Scheduler::UpdateLastContextSwitchTime(Thread* thread, Process* process) {
    const u64 prev_switch_ticks = last_context_switch_time;
    const u64 most_recent_switch_ticks = system.CoreTiming().GetCPUTicks();
    const u64 update_ticks = most_recent_switch_ticks - prev_switch_ticks;

    if (thread != nullptr) {
        thread->UpdateCPUTimeTicks(update_ticks);
    }

    if (process != nullptr) {
        process->UpdateCPUTimeTicks(update_ticks);
    }

    last_context_switch_time = most_recent_switch_ticks;
}

void Scheduler::Initialize() {
    std::string name = "Idle Thread Id:" + std::to_string(core_id);
    std::function<void(void*)> init_func = system.GetCpuManager().GetIdleThreadStartFunc();
    void* init_func_parameter = system.GetCpuManager().GetStartFuncParamater();
    ThreadType type = static_cast<ThreadType>(THREADTYPE_KERNEL | THREADTYPE_HLE | THREADTYPE_IDLE);
    auto thread_res = Thread::Create(system, type, name, 0, 64, 0, static_cast<u32>(core_id), 0,
                                     nullptr, std::move(init_func), init_func_parameter);
    idle_thread = std::move(thread_res).Unwrap();
}

void Scheduler::Shutdown() {
    current_thread = nullptr;
    selected_thread = nullptr;
}

SchedulerLock::SchedulerLock(KernelCore& kernel) : kernel{kernel} {
    kernel.GlobalScheduler().Lock();
}

SchedulerLock::~SchedulerLock() {
    kernel.GlobalScheduler().Unlock();
}

SchedulerLockAndSleep::SchedulerLockAndSleep(KernelCore& kernel, Handle& event_handle,
                                             Thread* time_task, s64 nanoseconds)
    : SchedulerLock{kernel}, event_handle{event_handle}, time_task{time_task}, nanoseconds{
                                                                                   nanoseconds} {
    event_handle = InvalidHandle;
}

SchedulerLockAndSleep::~SchedulerLockAndSleep() {
    if (sleep_cancelled) {
        return;
    }
    auto& time_manager = kernel.TimeManager();
    time_manager.ScheduleTimeEvent(event_handle, time_task, nanoseconds);
}

void SchedulerLockAndSleep::Release() {
    if (sleep_cancelled) {
        return;
    }
    auto& time_manager = kernel.TimeManager();
    time_manager.ScheduleTimeEvent(event_handle, time_task, nanoseconds);
    sleep_cancelled = true;
}

} // namespace Kernel
