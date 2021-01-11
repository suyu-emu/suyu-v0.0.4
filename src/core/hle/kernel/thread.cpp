// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <optional>
#include <vector>

#include "common/assert.h"
#include "common/common_types.h"
#include "common/fiber.h"
#include "common/logging/log.h"
#include "common/thread_queue_list.h"
#include "core/core.h"
#include "core/cpu_manager.h"
#include "core/hardware_properties.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/k_condition_variable.h"
#include "core/hle/kernel/k_scheduler.h"
#include "core/hle/kernel/k_scoped_scheduler_lock_and_sleep.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/memory/memory_layout.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/time_manager.h"
#include "core/hle/result.h"
#include "core/memory.h"

#ifdef ARCHITECTURE_x86_64
#include "core/arm/dynarmic/arm_dynarmic_32.h"
#include "core/arm/dynarmic/arm_dynarmic_64.h"
#endif

namespace Kernel {

bool Thread::IsSignaled() const {
    return signaled;
}

Thread::Thread(KernelCore& kernel) : KSynchronizationObject{kernel} {}
Thread::~Thread() = default;

void Thread::Stop() {
    {
        KScopedSchedulerLock lock(kernel);
        SetState(ThreadState::Terminated);
        signaled = true;
        NotifyAvailable();
        kernel.GlobalHandleTable().Close(global_handle);

        if (owner_process) {
            owner_process->UnregisterThread(this);

            // Mark the TLS slot in the thread's page as free.
            owner_process->FreeTLSRegion(tls_address);
        }
        has_exited = true;
    }
    global_handle = 0;
}

void Thread::Wakeup() {
    KScopedSchedulerLock lock(kernel);
    SetState(ThreadState::Runnable);
}

ResultCode Thread::Start() {
    KScopedSchedulerLock lock(kernel);
    SetState(ThreadState::Runnable);
    return RESULT_SUCCESS;
}

void Thread::CancelWait() {
    KScopedSchedulerLock lock(kernel);
    if (GetState() != ThreadState::Waiting || !is_cancellable) {
        is_sync_cancelled = true;
        return;
    }
    // TODO(Blinkhawk): Implement cancel of server session
    is_sync_cancelled = false;
    SetSynchronizationResults(nullptr, ERR_SYNCHRONIZATION_CANCELED);
    SetState(ThreadState::Runnable);
}

static void ResetThreadContext32(Core::ARM_Interface::ThreadContext32& context, u32 stack_top,
                                 u32 entry_point, u32 arg) {
    context = {};
    context.cpu_registers[0] = arg;
    context.cpu_registers[15] = entry_point;
    context.cpu_registers[13] = stack_top;
}

static void ResetThreadContext64(Core::ARM_Interface::ThreadContext64& context, VAddr stack_top,
                                 VAddr entry_point, u64 arg) {
    context = {};
    context.cpu_registers[0] = arg;
    context.pc = entry_point;
    context.sp = stack_top;
    // TODO(merry): Perform a hardware test to determine the below value.
    context.fpcr = 0;
}

std::shared_ptr<Common::Fiber>& Thread::GetHostContext() {
    return host_context;
}

ResultVal<std::shared_ptr<Thread>> Thread::Create(Core::System& system, ThreadType type_flags,
                                                  std::string name, VAddr entry_point, u32 priority,
                                                  u64 arg, s32 processor_id, VAddr stack_top,
                                                  Process* owner_process) {
    std::function<void(void*)> init_func = Core::CpuManager::GetGuestThreadStartFunc();
    void* init_func_parameter = system.GetCpuManager().GetStartFuncParamater();
    return Create(system, type_flags, name, entry_point, priority, arg, processor_id, stack_top,
                  owner_process, std::move(init_func), init_func_parameter);
}

ResultVal<std::shared_ptr<Thread>> Thread::Create(Core::System& system, ThreadType type_flags,
                                                  std::string name, VAddr entry_point, u32 priority,
                                                  u64 arg, s32 processor_id, VAddr stack_top,
                                                  Process* owner_process,
                                                  std::function<void(void*)>&& thread_start_func,
                                                  void* thread_start_parameter) {
    auto& kernel = system.Kernel();
    // Check if priority is in ranged. Lowest priority -> highest priority id.
    if (priority > THREADPRIO_LOWEST && ((type_flags & THREADTYPE_IDLE) == 0)) {
        LOG_ERROR(Kernel_SVC, "Invalid thread priority: {}", priority);
        return ERR_INVALID_THREAD_PRIORITY;
    }

    if (processor_id > THREADPROCESSORID_MAX) {
        LOG_ERROR(Kernel_SVC, "Invalid processor id: {}", processor_id);
        return ERR_INVALID_PROCESSOR_ID;
    }

    if (owner_process) {
        if (!system.Memory().IsValidVirtualAddress(*owner_process, entry_point)) {
            LOG_ERROR(Kernel_SVC, "(name={}): invalid entry {:016X}", name, entry_point);
            // TODO (bunnei): Find the correct error code to use here
            return RESULT_UNKNOWN;
        }
    }

    std::shared_ptr<Thread> thread = std::make_shared<Thread>(kernel);

    thread->thread_id = kernel.CreateNewThreadID();
    thread->thread_state = ThreadState::Initialized;
    thread->entry_point = entry_point;
    thread->stack_top = stack_top;
    thread->disable_count = 1;
    thread->tpidr_el0 = 0;
    thread->current_priority = priority;
    thread->base_priority = priority;
    thread->lock_owner = nullptr;
    thread->schedule_count = -1;
    thread->last_scheduled_tick = 0;
    thread->processor_id = processor_id;
    thread->ideal_core = processor_id;
    thread->affinity_mask.SetAffinity(processor_id, true);
    thread->name = std::move(name);
    thread->global_handle = kernel.GlobalHandleTable().Create(thread).Unwrap();
    thread->owner_process = owner_process;
    thread->type = type_flags;
    thread->signaled = false;
    if ((type_flags & THREADTYPE_IDLE) == 0) {
        auto& scheduler = kernel.GlobalSchedulerContext();
        scheduler.AddThread(thread);
    }
    if (owner_process) {
        thread->tls_address = thread->owner_process->CreateTLSRegion();
        thread->owner_process->RegisterThread(thread.get());
    } else {
        thread->tls_address = 0;
    }

    // TODO(peachum): move to ScheduleThread() when scheduler is added so selected core is used
    // to initialize the context
    if ((type_flags & THREADTYPE_HLE) == 0) {
        ResetThreadContext32(thread->context_32, static_cast<u32>(stack_top),
                             static_cast<u32>(entry_point), static_cast<u32>(arg));
        ResetThreadContext64(thread->context_64, stack_top, entry_point, arg);
    }
    thread->host_context =
        std::make_shared<Common::Fiber>(std::move(thread_start_func), thread_start_parameter);

    return MakeResult<std::shared_ptr<Thread>>(std::move(thread));
}

void Thread::SetBasePriority(u32 priority) {
    ASSERT_MSG(priority <= THREADPRIO_LOWEST && priority >= THREADPRIO_HIGHEST,
               "Invalid priority value.");

    KScopedSchedulerLock lock(kernel);

    // Change our base priority.
    base_priority = priority;

    // Perform a priority restoration.
    RestorePriority(kernel, this);
}

void Thread::SetSynchronizationResults(KSynchronizationObject* object, ResultCode result) {
    signaling_object = object;
    signaling_result = result;
}

VAddr Thread::GetCommandBufferAddress() const {
    // Offset from the start of TLS at which the IPC command buffer begins.
    constexpr u64 command_header_offset = 0x80;
    return GetTLSAddress() + command_header_offset;
}

void Thread::SetState(ThreadState state) {
    KScopedSchedulerLock sl(kernel);

    // Clear debugging state
    SetMutexWaitAddressForDebugging({});
    SetWaitReasonForDebugging({});

    const ThreadState old_state = thread_state;
    thread_state =
        static_cast<ThreadState>((old_state & ~ThreadState::Mask) | (state & ThreadState::Mask));
    if (thread_state != old_state) {
        KScheduler::OnThreadStateChanged(kernel, this, old_state);
    }
}

void Thread::AddWaiterImpl(Thread* thread) {
    ASSERT(kernel.GlobalSchedulerContext().IsLocked());

    // Find the right spot to insert the waiter.
    auto it = waiter_list.begin();
    while (it != waiter_list.end()) {
        if (it->GetPriority() > thread->GetPriority()) {
            break;
        }
        it++;
    }

    // Keep track of how many kernel waiters we have.
    if (Memory::IsKernelAddressKey(thread->GetAddressKey())) {
        ASSERT((num_kernel_waiters++) >= 0);
    }

    // Insert the waiter.
    waiter_list.insert(it, *thread);
    thread->SetLockOwner(this);
}

void Thread::RemoveWaiterImpl(Thread* thread) {
    ASSERT(kernel.GlobalSchedulerContext().IsLocked());

    // Keep track of how many kernel waiters we have.
    if (Memory::IsKernelAddressKey(thread->GetAddressKey())) {
        ASSERT((num_kernel_waiters--) > 0);
    }

    // Remove the waiter.
    waiter_list.erase(waiter_list.iterator_to(*thread));
    thread->SetLockOwner(nullptr);
}

void Thread::RestorePriority(KernelCore& kernel, Thread* thread) {
    ASSERT(kernel.GlobalSchedulerContext().IsLocked());

    while (true) {
        // We want to inherit priority where possible.
        s32 new_priority = thread->GetBasePriority();
        if (thread->HasWaiters()) {
            new_priority = std::min(new_priority, thread->waiter_list.front().GetPriority());
        }

        // If the priority we would inherit is not different from ours, don't do anything.
        if (new_priority == thread->GetPriority()) {
            return;
        }

        // Ensure we don't violate condition variable red black tree invariants.
        if (auto* cv_tree = thread->GetConditionVariableTree(); cv_tree != nullptr) {
            BeforeUpdatePriority(kernel, cv_tree, thread);
        }

        // Change the priority.
        const s32 old_priority = thread->GetPriority();
        thread->SetPriority(new_priority);

        // Restore the condition variable, if relevant.
        if (auto* cv_tree = thread->GetConditionVariableTree(); cv_tree != nullptr) {
            AfterUpdatePriority(kernel, cv_tree, thread);
        }

        // Update the scheduler.
        KScheduler::OnThreadPriorityChanged(kernel, thread, old_priority);

        // Keep the lock owner up to date.
        Thread* lock_owner = thread->GetLockOwner();
        if (lock_owner == nullptr) {
            return;
        }

        // Update the thread in the lock owner's sorted list, and continue inheriting.
        lock_owner->RemoveWaiterImpl(thread);
        lock_owner->AddWaiterImpl(thread);
        thread = lock_owner;
    }
}

void Thread::AddWaiter(Thread* thread) {
    AddWaiterImpl(thread);
    RestorePriority(kernel, this);
}

void Thread::RemoveWaiter(Thread* thread) {
    RemoveWaiterImpl(thread);
    RestorePriority(kernel, this);
}

Thread* Thread::RemoveWaiterByKey(s32* out_num_waiters, VAddr key) {
    ASSERT(kernel.GlobalSchedulerContext().IsLocked());

    s32 num_waiters{};
    Thread* next_lock_owner{};
    auto it = waiter_list.begin();
    while (it != waiter_list.end()) {
        if (it->GetAddressKey() == key) {
            Thread* thread = std::addressof(*it);

            // Keep track of how many kernel waiters we have.
            if (Memory::IsKernelAddressKey(thread->GetAddressKey())) {
                ASSERT((num_kernel_waiters--) > 0);
            }
            it = waiter_list.erase(it);

            // Update the next lock owner.
            if (next_lock_owner == nullptr) {
                next_lock_owner = thread;
                next_lock_owner->SetLockOwner(nullptr);
            } else {
                next_lock_owner->AddWaiterImpl(thread);
            }
            num_waiters++;
        } else {
            it++;
        }
    }

    // Do priority updates, if we have a next owner.
    if (next_lock_owner) {
        RestorePriority(kernel, this);
        RestorePriority(kernel, next_lock_owner);
    }

    // Return output.
    *out_num_waiters = num_waiters;
    return next_lock_owner;
}

ResultCode Thread::SetActivity(ThreadActivity value) {
    KScopedSchedulerLock lock(kernel);

    auto sched_status = GetState();

    if (sched_status != ThreadState::Runnable && sched_status != ThreadState::Waiting) {
        return ERR_INVALID_STATE;
    }

    if (IsTerminationRequested()) {
        return RESULT_SUCCESS;
    }

    if (value == ThreadActivity::Paused) {
        if ((pausing_state & static_cast<u32>(ThreadSchedFlags::ThreadPauseFlag)) != 0) {
            return ERR_INVALID_STATE;
        }
        AddSchedulingFlag(ThreadSchedFlags::ThreadPauseFlag);
    } else {
        if ((pausing_state & static_cast<u32>(ThreadSchedFlags::ThreadPauseFlag)) == 0) {
            return ERR_INVALID_STATE;
        }
        RemoveSchedulingFlag(ThreadSchedFlags::ThreadPauseFlag);
    }
    return RESULT_SUCCESS;
}

ResultCode Thread::Sleep(s64 nanoseconds) {
    Handle event_handle{};
    {
        KScopedSchedulerLockAndSleep lock(kernel, event_handle, this, nanoseconds);
        SetState(ThreadState::Waiting);
        SetWaitReasonForDebugging(ThreadWaitReasonForDebugging::Sleep);
    }

    if (event_handle != InvalidHandle) {
        auto& time_manager = kernel.TimeManager();
        time_manager.UnscheduleTimeEvent(event_handle);
    }
    return RESULT_SUCCESS;
}

void Thread::AddSchedulingFlag(ThreadSchedFlags flag) {
    const auto old_state = GetRawState();
    pausing_state |= static_cast<u32>(flag);
    const auto base_scheduling = GetState();
    thread_state = base_scheduling | static_cast<ThreadState>(pausing_state);
    KScheduler::OnThreadStateChanged(kernel, this, old_state);
}

void Thread::RemoveSchedulingFlag(ThreadSchedFlags flag) {
    const auto old_state = GetRawState();
    pausing_state &= ~static_cast<u32>(flag);
    const auto base_scheduling = GetState();
    thread_state = base_scheduling | static_cast<ThreadState>(pausing_state);
    KScheduler::OnThreadStateChanged(kernel, this, old_state);
}

ResultCode Thread::SetCoreAndAffinityMask(s32 new_core, u64 new_affinity_mask) {
    KScopedSchedulerLock lock(kernel);
    const auto HighestSetCore = [](u64 mask, u32 max_cores) {
        for (s32 core = static_cast<s32>(max_cores - 1); core >= 0; core--) {
            if (((mask >> core) & 1) != 0) {
                return core;
            }
        }
        return -1;
    };

    const bool use_override = affinity_override_count != 0;
    if (new_core == THREADPROCESSORID_DONT_UPDATE) {
        new_core = use_override ? ideal_core_override : ideal_core;
        if ((new_affinity_mask & (1ULL << new_core)) == 0) {
            LOG_ERROR(Kernel, "New affinity mask is incorrect! new_core={}, new_affinity_mask={}",
                      new_core, new_affinity_mask);
            return ERR_INVALID_COMBINATION;
        }
    }
    if (use_override) {
        ideal_core_override = new_core;
    } else {
        const auto old_affinity_mask = affinity_mask;
        affinity_mask.SetAffinityMask(new_affinity_mask);
        ideal_core = new_core;
        if (old_affinity_mask.GetAffinityMask() != new_affinity_mask) {
            const s32 old_core = processor_id;
            if (processor_id >= 0 && !affinity_mask.GetAffinity(processor_id)) {
                if (static_cast<s32>(ideal_core) < 0) {
                    processor_id = HighestSetCore(affinity_mask.GetAffinityMask(),
                                                  Core::Hardware::NUM_CPU_CORES);
                } else {
                    processor_id = ideal_core;
                }
            }
            KScheduler::OnThreadAffinityMaskChanged(kernel, this, old_affinity_mask, old_core);
        }
    }
    return RESULT_SUCCESS;
}

} // namespace Kernel
