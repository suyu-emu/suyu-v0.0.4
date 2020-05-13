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
#include "core/arm/arm_interface.h"
#ifdef ARCHITECTURE_x86_64
#include "core/arm/dynarmic/arm_dynarmic_32.h"
#include "core/arm/dynarmic/arm_dynarmic_64.h"
#endif
#include "core/arm/cpu_interrupt_handler.h"
#include "core/arm/exclusive_monitor.h"
#include "core/arm/unicorn/arm_unicorn.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/core_timing_util.h"
#include "core/cpu_manager.h"
#include "core/hardware_properties.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/scheduler.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/time_manager.h"
#include "core/hle/result.h"
#include "core/memory.h"

namespace Kernel {

bool Thread::ShouldWait(const Thread* thread) const {
    return status != ThreadStatus::Dead;
}

bool Thread::IsSignaled() const {
    return status == ThreadStatus::Dead;
}

void Thread::Acquire(Thread* thread) {
    ASSERT_MSG(!ShouldWait(thread), "object unavailable!");
}

Thread::Thread(KernelCore& kernel) : SynchronizationObject{kernel} {}
Thread::~Thread() = default;

void Thread::Stop() {
    {
        SchedulerLock lock(kernel);
        SetStatus(ThreadStatus::Dead);
        Signal();
        kernel.GlobalHandleTable().Close(global_handle);

        if (owner_process) {
            owner_process->UnregisterThread(this);

            // Mark the TLS slot in the thread's page as free.
            owner_process->FreeTLSRegion(tls_address);
        }
        arm_interface.reset();
        has_exited = true;
    }
    global_handle = 0;
}

void Thread::ResumeFromWait() {
    SchedulerLock lock(kernel);
    switch (status) {
    case ThreadStatus::Paused:
    case ThreadStatus::WaitSynch:
    case ThreadStatus::WaitHLEEvent:
    case ThreadStatus::WaitSleep:
    case ThreadStatus::WaitIPC:
    case ThreadStatus::WaitMutex:
    case ThreadStatus::WaitCondVar:
    case ThreadStatus::WaitArb:
    case ThreadStatus::Dormant:
        break;

    case ThreadStatus::Ready:
        // The thread's wakeup callback must have already been cleared when the thread was first
        // awoken.
        ASSERT(hle_callback == nullptr);
        // If the thread is waiting on multiple wait objects, it might be awoken more than once
        // before actually resuming. We can ignore subsequent wakeups if the thread status has
        // already been set to ThreadStatus::Ready.
        return;

    case ThreadStatus::Running:
        DEBUG_ASSERT_MSG(false, "Thread with object id {} has already resumed.", GetObjectId());
        return;
    case ThreadStatus::Dead:
        // This should never happen, as threads must complete before being stopped.
        DEBUG_ASSERT_MSG(false, "Thread with object id {} cannot be resumed because it's DEAD.",
                         GetObjectId());
        return;
    }

    SetStatus(ThreadStatus::Ready);
}

void Thread::OnWakeUp() {
    SchedulerLock lock(kernel);

    SetStatus(ThreadStatus::Ready);
}

ResultCode Thread::Start() {
    SchedulerLock lock(kernel);
    SetStatus(ThreadStatus::Ready);
    return RESULT_SUCCESS;
}

void Thread::CancelWait() {
    SchedulerLock lock(kernel);
    if (GetSchedulingStatus() != ThreadSchedStatus::Paused || !is_waiting_on_sync) {
        is_sync_cancelled = true;
        return;
    }
    // TODO(Blinkhawk): Implement cancel of server session
    is_sync_cancelled = false;
    SetSynchronizationResults(nullptr, ERR_SYNCHRONIZATION_CANCELED);
    SetStatus(ThreadStatus::Ready);
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
    std::function<void(void*)> init_func = system.GetCpuManager().GetGuestThreadStartFunc();
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
    thread->status = ThreadStatus::Dormant;
    thread->entry_point = entry_point;
    thread->stack_top = stack_top;
    thread->tpidr_el0 = 0;
    thread->nominal_priority = thread->current_priority = priority;
    thread->last_running_ticks = 0;
    thread->processor_id = processor_id;
    thread->ideal_core = processor_id;
    thread->affinity_mask = 1ULL << processor_id;
    thread->wait_objects = nullptr;
    thread->mutex_wait_address = 0;
    thread->condvar_wait_address = 0;
    thread->wait_handle = 0;
    thread->name = std::move(name);
    thread->global_handle = kernel.GlobalHandleTable().Create(thread).Unwrap();
    thread->owner_process = owner_process;
    thread->type = type_flags;
    if ((type_flags & THREADTYPE_IDLE) == 0) {
        auto& scheduler = kernel.GlobalScheduler();
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
    thread->arm_interface.reset();
    if ((type_flags & THREADTYPE_HLE) == 0) {
#ifdef ARCHITECTURE_x86_64
        if (owner_process && !owner_process->Is64BitProcess()) {
            thread->arm_interface = std::make_unique<Core::ARM_Dynarmic_32>(
                system, kernel.Interrupts(), kernel.IsMulticore(), kernel.GetExclusiveMonitor(),
                processor_id);
        } else {
            thread->arm_interface = std::make_unique<Core::ARM_Dynarmic_64>(
                system, kernel.Interrupts(), kernel.IsMulticore(), kernel.GetExclusiveMonitor(),
                processor_id);
        }

#else
        if (owner_process && !owner_process->Is64BitProcess()) {
            thread->arm_interface = std::make_shared<Core::ARM_Unicorn>(
                system, kernel.Interrupts(), kernel.IsMulticore(), ARM_Unicorn::Arch::AArch32,
                processor_id);
        } else {
            thread->arm_interface = std::make_shared<Core::ARM_Unicorn>(
                system, kernel.Interrupts(), kernel.IsMulticore(), ARM_Unicorn::Arch::AArch64,
                processor_id);
        }
        LOG_WARNING(Core, "CPU JIT requested, but Dynarmic not available");
#endif
        ResetThreadContext32(thread->context_32, static_cast<u32>(stack_top),
                             static_cast<u32>(entry_point), static_cast<u32>(arg));
        ResetThreadContext64(thread->context_64, stack_top, entry_point, arg);
    }
    thread->host_context =
        std::make_shared<Common::Fiber>(std::move(thread_start_func), thread_start_parameter);

    return MakeResult<std::shared_ptr<Thread>>(std::move(thread));
}

void Thread::SetPriority(u32 priority) {
    SchedulerLock lock(kernel);
    ASSERT_MSG(priority <= THREADPRIO_LOWEST && priority >= THREADPRIO_HIGHEST,
               "Invalid priority value.");
    nominal_priority = priority;
    UpdatePriority();
}

void Thread::SetSynchronizationResults(SynchronizationObject* object, ResultCode result) {
    signaling_object = object;
    signaling_result = result;
}

s32 Thread::GetSynchronizationObjectIndex(std::shared_ptr<SynchronizationObject> object) const {
    ASSERT_MSG(!wait_objects->empty(), "Thread is not waiting for anything");
    const auto match = std::find(wait_objects->rbegin(), wait_objects->rend(), object);
    return static_cast<s32>(std::distance(match, wait_objects->rend()) - 1);
}

VAddr Thread::GetCommandBufferAddress() const {
    // Offset from the start of TLS at which the IPC command buffer begins.
    constexpr u64 command_header_offset = 0x80;
    return GetTLSAddress() + command_header_offset;
}

Core::ARM_Interface& Thread::ArmInterface() {
    return *arm_interface;
}

const Core::ARM_Interface& Thread::ArmInterface() const {
    return *arm_interface;
}

void Thread::SetStatus(ThreadStatus new_status) {
    if (new_status == status) {
        return;
    }

    switch (new_status) {
    case ThreadStatus::Ready:
    case ThreadStatus::Running:
        SetSchedulingStatus(ThreadSchedStatus::Runnable);
        break;
    case ThreadStatus::Dormant:
        SetSchedulingStatus(ThreadSchedStatus::None);
        break;
    case ThreadStatus::Dead:
        SetSchedulingStatus(ThreadSchedStatus::Exited);
        break;
    default:
        SetSchedulingStatus(ThreadSchedStatus::Paused);
        break;
    }

    status = new_status;
}

void Thread::AddMutexWaiter(std::shared_ptr<Thread> thread) {
    if (thread->lock_owner.get() == this) {
        // If the thread is already waiting for this thread to release the mutex, ensure that the
        // waiters list is consistent and return without doing anything.
        const auto iter = std::find(wait_mutex_threads.begin(), wait_mutex_threads.end(), thread);
        ASSERT(iter != wait_mutex_threads.end());
        return;
    }

    // A thread can't wait on two different mutexes at the same time.
    ASSERT(thread->lock_owner == nullptr);

    // Ensure that the thread is not already in the list of mutex waiters
    const auto iter = std::find(wait_mutex_threads.begin(), wait_mutex_threads.end(), thread);
    ASSERT(iter == wait_mutex_threads.end());

    // Keep the list in an ordered fashion
    const auto insertion_point = std::find_if(
        wait_mutex_threads.begin(), wait_mutex_threads.end(),
        [&thread](const auto& entry) { return entry->GetPriority() > thread->GetPriority(); });
    wait_mutex_threads.insert(insertion_point, thread);
    thread->lock_owner = SharedFrom(this);

    UpdatePriority();
}

void Thread::RemoveMutexWaiter(std::shared_ptr<Thread> thread) {
    ASSERT(thread->lock_owner.get() == this);

    // Ensure that the thread is in the list of mutex waiters
    const auto iter = std::find(wait_mutex_threads.begin(), wait_mutex_threads.end(), thread);
    ASSERT(iter != wait_mutex_threads.end());

    wait_mutex_threads.erase(iter);

    thread->lock_owner = nullptr;
    UpdatePriority();
}

void Thread::UpdatePriority() {
    // If any of the threads waiting on the mutex have a higher priority
    // (taking into account priority inheritance), then this thread inherits
    // that thread's priority.
    u32 new_priority = nominal_priority;
    if (!wait_mutex_threads.empty()) {
        if (wait_mutex_threads.front()->current_priority < new_priority) {
            new_priority = wait_mutex_threads.front()->current_priority;
        }
    }

    if (new_priority == current_priority) {
        return;
    }

    if (GetStatus() == ThreadStatus::WaitCondVar) {
        owner_process->RemoveConditionVariableThread(SharedFrom(this));
    }

    SetCurrentPriority(new_priority);

    if (GetStatus() == ThreadStatus::WaitCondVar) {
        owner_process->InsertConditionVariableThread(SharedFrom(this));
    }

    if (!lock_owner) {
        return;
    }

    // Ensure that the thread is within the correct location in the waiting list.
    auto old_owner = lock_owner;
    lock_owner->RemoveMutexWaiter(SharedFrom(this));
    old_owner->AddMutexWaiter(SharedFrom(this));

    // Recursively update the priority of the thread that depends on the priority of this one.
    lock_owner->UpdatePriority();
}

bool Thread::AllSynchronizationObjectsReady() const {
    return std::none_of(wait_objects->begin(), wait_objects->end(),
                        [this](const std::shared_ptr<SynchronizationObject>& object) {
                            return object->ShouldWait(this);
                        });
}

bool Thread::InvokeHLECallback(std::shared_ptr<Thread> thread) {
    ASSERT(hle_callback);
    return hle_callback(std::move(thread));
}

ResultCode Thread::SetActivity(ThreadActivity value) {
    SchedulerLock lock(kernel);

    auto sched_status = GetSchedulingStatus();

    if (sched_status != ThreadSchedStatus::Runnable && sched_status != ThreadSchedStatus::Paused) {
        return ERR_INVALID_STATE;
    }

    if (IsPendingTermination()) {
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
        SchedulerLockAndSleep lock(kernel, event_handle, this, nanoseconds);
        SetStatus(ThreadStatus::WaitSleep);
    }

    if (event_handle != InvalidHandle) {
        auto& time_manager = kernel.TimeManager();
        time_manager.UnscheduleTimeEvent(event_handle);
    }
    return RESULT_SUCCESS;
}

std::pair<ResultCode, bool> Thread::YieldSimple() {
    bool is_redundant = false;
    {
        SchedulerLock lock(kernel);
        is_redundant = kernel.GlobalScheduler().YieldThread(this);
    }
    return {RESULT_SUCCESS, is_redundant};
}

std::pair<ResultCode, bool> Thread::YieldAndBalanceLoad() {
    bool is_redundant = false;
    {
        SchedulerLock lock(kernel);
        is_redundant = kernel.GlobalScheduler().YieldThreadAndBalanceLoad(this);
    }
    return {RESULT_SUCCESS, is_redundant};
}

std::pair<ResultCode, bool> Thread::YieldAndWaitForLoadBalancing() {
    bool is_redundant = false;
    {
        SchedulerLock lock(kernel);
        is_redundant = kernel.GlobalScheduler().YieldThreadAndWaitForLoadBalancing(this);
    }
    return {RESULT_SUCCESS, is_redundant};
}

void Thread::AddSchedulingFlag(ThreadSchedFlags flag) {
    const u32 old_state = scheduling_state;
    pausing_state |= static_cast<u32>(flag);
    const u32 base_scheduling = static_cast<u32>(GetSchedulingStatus());
    scheduling_state = base_scheduling | pausing_state;
    kernel.GlobalScheduler().AdjustSchedulingOnStatus(this, old_state);
}

void Thread::RemoveSchedulingFlag(ThreadSchedFlags flag) {
    const u32 old_state = scheduling_state;
    pausing_state &= ~static_cast<u32>(flag);
    const u32 base_scheduling = static_cast<u32>(GetSchedulingStatus());
    scheduling_state = base_scheduling | pausing_state;
    kernel.GlobalScheduler().AdjustSchedulingOnStatus(this, old_state);
}

void Thread::SetSchedulingStatus(ThreadSchedStatus new_status) {
    const u32 old_state = scheduling_state;
    scheduling_state = (scheduling_state & static_cast<u32>(ThreadSchedMasks::HighMask)) |
                       static_cast<u32>(new_status);
    kernel.GlobalScheduler().AdjustSchedulingOnStatus(this, old_state);
}

void Thread::SetCurrentPriority(u32 new_priority) {
    const u32 old_priority = std::exchange(current_priority, new_priority);
    kernel.GlobalScheduler().AdjustSchedulingOnPriority(this, old_priority);
}

ResultCode Thread::SetCoreAndAffinityMask(s32 new_core, u64 new_affinity_mask) {
    SchedulerLock lock(kernel);
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
        affinity_mask_override = new_affinity_mask;
    } else {
        const u64 old_affinity_mask = std::exchange(affinity_mask, new_affinity_mask);
        ideal_core = new_core;
        if (old_affinity_mask != new_affinity_mask) {
            const s32 old_core = processor_id;
            if (processor_id >= 0 && ((affinity_mask >> processor_id) & 1) == 0) {
                if (static_cast<s32>(ideal_core) < 0) {
                    processor_id = HighestSetCore(affinity_mask, Core::Hardware::NUM_CPU_CORES);
                } else {
                    processor_id = ideal_core;
                }
            }
            kernel.GlobalScheduler().AdjustSchedulingOnAffinity(this, old_affinity_mask, old_core);
        }
    }
    return RESULT_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Gets the current thread
 */
Thread* GetCurrentThread() {
    return Core::System::GetInstance().CurrentScheduler().GetCurrentThread();
}

} // namespace Kernel
