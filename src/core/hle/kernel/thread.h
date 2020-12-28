// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "common/common_types.h"
#include "common/spin_lock.h"
#include "core/arm/arm_interface.h"
#include "core/hle/kernel/k_affinity_mask.h"
#include "core/hle/kernel/k_synchronization_object.h"
#include "core/hle/kernel/object.h"
#include "core/hle/result.h"

namespace Common {
class Fiber;
}

namespace Core {
class ARM_Interface;
class System;
} // namespace Core

namespace Kernel {

class GlobalSchedulerContext;
class KernelCore;
class Process;
class KScheduler;

enum ThreadPriority : u32 {
    THREADPRIO_HIGHEST = 0,            ///< Highest thread priority
    THREADPRIO_MAX_CORE_MIGRATION = 2, ///< Highest priority for a core migration
    THREADPRIO_USERLAND_MAX = 24,      ///< Highest thread priority for userland apps
    THREADPRIO_DEFAULT = 44,           ///< Default thread priority for userland apps
    THREADPRIO_LOWEST = 63,            ///< Lowest thread priority
    THREADPRIO_COUNT = 64,             ///< Total number of possible thread priorities.
};

enum ThreadType : u32 {
    THREADTYPE_USER = 0x1,
    THREADTYPE_KERNEL = 0x2,
    THREADTYPE_HLE = 0x4,
    THREADTYPE_IDLE = 0x8,
    THREADTYPE_SUSPEND = 0x10,
};

enum ThreadProcessorId : s32 {
    /// Indicates that no particular processor core is preferred.
    THREADPROCESSORID_DONT_CARE = -1,

    /// Run thread on the ideal core specified by the process.
    THREADPROCESSORID_IDEAL = -2,

    /// Indicates that the preferred processor ID shouldn't be updated in
    /// a core mask setting operation.
    THREADPROCESSORID_DONT_UPDATE = -3,

    THREADPROCESSORID_0 = 0,   ///< Run thread on core 0
    THREADPROCESSORID_1 = 1,   ///< Run thread on core 1
    THREADPROCESSORID_2 = 2,   ///< Run thread on core 2
    THREADPROCESSORID_3 = 3,   ///< Run thread on core 3
    THREADPROCESSORID_MAX = 4, ///< Processor ID must be less than this

    /// Allowed CPU mask
    THREADPROCESSORID_DEFAULT_MASK = (1 << THREADPROCESSORID_0) | (1 << THREADPROCESSORID_1) |
                                     (1 << THREADPROCESSORID_2) | (1 << THREADPROCESSORID_3)
};

enum class ThreadState : u16 {
    Initialized = 0,
    Waiting = 1,
    Runnable = 2,
    Terminated = 3,

    SuspendShift = 4,
    Mask = (1 << SuspendShift) - 1,

    ProcessSuspended = (1 << (0 + SuspendShift)),
    ThreadSuspended = (1 << (1 + SuspendShift)),
    DebugSuspended = (1 << (2 + SuspendShift)),
    BacktraceSuspended = (1 << (3 + SuspendShift)),
    InitSuspended = (1 << (4 + SuspendShift)),

    SuspendFlagMask = ((1 << 5) - 1) << SuspendShift,

    HighMask = 0xfff0,
};
DECLARE_ENUM_FLAG_OPERATORS(ThreadState);

enum class ThreadWakeupReason {
    Signal, // The thread was woken up by WakeupAllWaitingThreads due to an object signal.
    Timeout // The thread was woken up due to a wait timeout.
};

enum class ThreadActivity : u32 {
    Normal = 0,
    Paused = 1,
};

enum class ThreadSchedFlags : u32 {
    ProcessPauseFlag = 1 << 4,
    ThreadPauseFlag = 1 << 5,
    ProcessDebugPauseFlag = 1 << 6,
    KernelInitPauseFlag = 1 << 8,
};

class Thread final : public KSynchronizationObject {
public:
    explicit Thread(KernelCore& kernel);
    ~Thread() override;

    using MutexWaitingThreads = std::vector<std::shared_ptr<Thread>>;

    using ThreadContext32 = Core::ARM_Interface::ThreadContext32;
    using ThreadContext64 = Core::ARM_Interface::ThreadContext64;

    /**
     * Creates and returns a new thread. The new thread is immediately scheduled
     * @param system The instance of the whole system
     * @param name The friendly name desired for the thread
     * @param entry_point The address at which the thread should start execution
     * @param priority The thread's priority
     * @param arg User data to pass to the thread
     * @param processor_id The ID(s) of the processors on which the thread is desired to be run
     * @param stack_top The address of the thread's stack top
     * @param owner_process The parent process for the thread, if null, it's a kernel thread
     * @return A shared pointer to the newly created thread
     */
    static ResultVal<std::shared_ptr<Thread>> Create(Core::System& system, ThreadType type_flags,
                                                     std::string name, VAddr entry_point,
                                                     u32 priority, u64 arg, s32 processor_id,
                                                     VAddr stack_top, Process* owner_process);

    /**
     * Creates and returns a new thread. The new thread is immediately scheduled
     * @param system The instance of the whole system
     * @param name The friendly name desired for the thread
     * @param entry_point The address at which the thread should start execution
     * @param priority The thread's priority
     * @param arg User data to pass to the thread
     * @param processor_id The ID(s) of the processors on which the thread is desired to be run
     * @param stack_top The address of the thread's stack top
     * @param owner_process The parent process for the thread, if null, it's a kernel thread
     * @param thread_start_func The function where the host context will start.
     * @param thread_start_parameter The parameter which will passed to host context on init
     * @return A shared pointer to the newly created thread
     */
    static ResultVal<std::shared_ptr<Thread>> Create(Core::System& system, ThreadType type_flags,
                                                     std::string name, VAddr entry_point,
                                                     u32 priority, u64 arg, s32 processor_id,
                                                     VAddr stack_top, Process* owner_process,
                                                     std::function<void(void*)>&& thread_start_func,
                                                     void* thread_start_parameter);

    std::string GetName() const override {
        return name;
    }

    void SetName(std::string new_name) {
        name = std::move(new_name);
    }

    std::string GetTypeName() const override {
        return "Thread";
    }

    static constexpr HandleType HANDLE_TYPE = HandleType::Thread;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    /**
     * Gets the thread's current priority
     * @return The current thread's priority
     */
    u32 GetPriority() const {
        return current_priority;
    }

    /**
     * Gets the thread's nominal priority.
     * @return The current thread's nominal priority.
     */
    u32 GetNominalPriority() const {
        return nominal_priority;
    }

    /**
     * Sets the thread's current priority
     * @param priority The new priority
     */
    void SetPriority(u32 priority);

    /// Adds a thread to the list of threads that are waiting for a lock held by this thread.
    void AddMutexWaiter(std::shared_ptr<Thread> thread);

    /// Removes a thread from the list of threads that are waiting for a lock held by this thread.
    void RemoveMutexWaiter(std::shared_ptr<Thread> thread);

    /// Recalculates the current priority taking into account priority inheritance.
    void UpdatePriority();

    /// Changes the core that the thread is running or scheduled to run on.
    ResultCode SetCoreAndAffinityMask(s32 new_core, u64 new_affinity_mask);

    /**
     * Gets the thread's thread ID
     * @return The thread's ID
     */
    u64 GetThreadID() const {
        return thread_id;
    }

    /// Resumes a thread from waiting
    void Wakeup();

    void OnWakeUp();

    ResultCode Start();

    virtual bool IsSignaled() const override;

    /// Cancels a waiting operation that this thread may or may not be within.
    ///
    /// When the thread is within a waiting state, this will set the thread's
    /// waiting result to signal a canceled wait. The function will then resume
    /// this thread.
    ///
    void CancelWait();

    void SetSynchronizationResults(KSynchronizationObject* object, ResultCode result);

    void SetSyncedObject(KSynchronizationObject* object, ResultCode result) {
        SetSynchronizationResults(object, result);
    }

    ResultCode GetWaitResult(KSynchronizationObject** out) const {
        *out = this->signaling_object;
        return signaling_result;
    }

    ResultCode GetSignalingResult() const {
        return signaling_result;
    }

    /**
     * Stops a thread, invalidating it from further use
     */
    void Stop();

    /*
     * Returns the Thread Local Storage address of the current thread
     * @returns VAddr of the thread's TLS
     */
    VAddr GetTLSAddress() const {
        return tls_address;
    }

    /*
     * Returns the value of the TPIDR_EL0 Read/Write system register for this thread.
     * @returns The value of the TPIDR_EL0 register.
     */
    u64 GetTPIDR_EL0() const {
        return tpidr_el0;
    }

    /// Sets the value of the TPIDR_EL0 Read/Write system register for this thread.
    void SetTPIDR_EL0(u64 value) {
        tpidr_el0 = value;
    }

    /*
     * Returns the address of the current thread's command buffer, located in the TLS.
     * @returns VAddr of the thread's command buffer.
     */
    VAddr GetCommandBufferAddress() const;

    ThreadContext32& GetContext32() {
        return context_32;
    }

    const ThreadContext32& GetContext32() const {
        return context_32;
    }

    ThreadContext64& GetContext64() {
        return context_64;
    }

    const ThreadContext64& GetContext64() const {
        return context_64;
    }

    bool IsHLEThread() const {
        return (type & THREADTYPE_HLE) != 0;
    }

    bool IsSuspendThread() const {
        return (type & THREADTYPE_SUSPEND) != 0;
    }

    bool IsIdleThread() const {
        return (type & THREADTYPE_IDLE) != 0;
    }

    bool WasRunning() const {
        return was_running;
    }

    void SetWasRunning(bool value) {
        was_running = value;
    }

    std::shared_ptr<Common::Fiber>& GetHostContext();

    ThreadState GetState() const {
        return thread_state & ThreadState::Mask;
    }

    ThreadState GetRawState() const {
        return thread_state;
    }

    void SetState(ThreadState new_state);

    void SetWaitingCondVar(bool value) {
        is_waiting_on_condvar = value;
    }

    s64 GetLastScheduledTick() const {
        return this->last_scheduled_tick;
    }

    void SetLastScheduledTick(s64 tick) {
        this->last_scheduled_tick = tick;
    }

    u64 GetTotalCPUTimeTicks() const {
        return total_cpu_time_ticks;
    }

    void UpdateCPUTimeTicks(u64 ticks) {
        total_cpu_time_ticks += ticks;
    }

    s32 GetProcessorID() const {
        return processor_id;
    }

    s32 GetActiveCore() const {
        return GetProcessorID();
    }

    void SetProcessorID(s32 new_core) {
        processor_id = new_core;
    }

    void SetActiveCore(s32 new_core) {
        processor_id = new_core;
    }

    Process* GetOwnerProcess() {
        return owner_process;
    }

    const Process* GetOwnerProcess() const {
        return owner_process;
    }

    const MutexWaitingThreads& GetMutexWaitingThreads() const {
        return wait_mutex_threads;
    }

    Thread* GetLockOwner() const {
        return lock_owner.get();
    }

    void SetLockOwner(std::shared_ptr<Thread> owner) {
        lock_owner = std::move(owner);
    }

    VAddr GetCondVarWaitAddress() const {
        return condvar_wait_address;
    }

    void SetCondVarWaitAddress(VAddr address) {
        condvar_wait_address = address;
    }

    VAddr GetMutexWaitAddress() const {
        return mutex_wait_address;
    }

    void SetMutexWaitAddress(VAddr address) {
        mutex_wait_address = address;
    }

    Handle GetWaitHandle() const {
        return wait_handle;
    }

    void SetWaitHandle(Handle handle) {
        wait_handle = handle;
    }

    VAddr GetArbiterWaitAddress() const {
        return arb_wait_address;
    }

    void SetArbiterWaitAddress(VAddr address) {
        arb_wait_address = address;
    }

    void SetHLETimeEvent(Handle time_event) {
        hle_time_event = time_event;
    }

    Handle GetHLETimeEvent() const {
        return hle_time_event;
    }

    bool InvokeHLECallback(std::shared_ptr<Thread> thread);

    u32 GetIdealCore() const {
        return ideal_core;
    }

    const KAffinityMask& GetAffinityMask() const {
        return affinity_mask;
    }

    ResultCode SetActivity(ThreadActivity value);

    /// Sleeps this thread for the given amount of nanoseconds.
    ResultCode Sleep(s64 nanoseconds);

    s64 GetYieldScheduleCount() const {
        return this->schedule_count;
    }

    void SetYieldScheduleCount(s64 count) {
        this->schedule_count = count;
    }

    bool IsRunning() const {
        return is_running;
    }

    void SetIsRunning(bool value) {
        is_running = value;
    }

    bool IsWaitCancelled() const {
        return is_sync_cancelled;
    }

    void ClearWaitCancelled() {
        is_sync_cancelled = false;
    }

    Handle GetGlobalHandle() const {
        return global_handle;
    }

    bool IsWaitingForArbitration() const {
        return waiting_for_arbitration;
    }

    void WaitForArbitration(bool set) {
        waiting_for_arbitration = set;
    }

    bool IsCancellable() const {
        return is_cancellable;
    }

    void SetCancellable() {
        is_cancellable = true;
    }

    void ClearCancellable() {
        is_cancellable = false;
    }

    bool IsTerminationRequested() const {
        return will_be_terminated || GetState() == ThreadState::Terminated;
    }

    bool IsPaused() const {
        return pausing_state != 0;
    }

    bool IsContinuousOnSVC() const {
        return is_continuous_on_svc;
    }

    void SetContinuousOnSVC(bool is_continuous) {
        is_continuous_on_svc = is_continuous;
    }

    bool IsPhantomMode() const {
        return is_phantom_mode;
    }

    void SetPhantomMode(bool phantom) {
        is_phantom_mode = phantom;
    }

    bool HasExited() const {
        return has_exited;
    }

    class QueueEntry {
    public:
        constexpr QueueEntry() = default;

        constexpr void Initialize() {
            this->prev = nullptr;
            this->next = nullptr;
        }

        constexpr Thread* GetPrev() const {
            return this->prev;
        }
        constexpr Thread* GetNext() const {
            return this->next;
        }
        constexpr void SetPrev(Thread* thread) {
            this->prev = thread;
        }
        constexpr void SetNext(Thread* thread) {
            this->next = thread;
        }

    private:
        Thread* prev{};
        Thread* next{};
    };

    QueueEntry& GetPriorityQueueEntry(s32 core) {
        return this->per_core_priority_queue_entry[core];
    }

    const QueueEntry& GetPriorityQueueEntry(s32 core) const {
        return this->per_core_priority_queue_entry[core];
    }

    s32 GetDisableDispatchCount() const {
        return disable_count;
    }

    void DisableDispatch() {
        ASSERT(GetDisableDispatchCount() >= 0);
        disable_count++;
    }

    void EnableDispatch() {
        ASSERT(GetDisableDispatchCount() > 0);
        disable_count--;
    }

    void SetWaitObjectsForDebugging(KSynchronizationObject** objects, s32 num_objects) {
        wait_objects_for_debugging.clear();
        wait_objects_for_debugging.reserve(num_objects);
        for (auto i = 0; i < num_objects; ++i) {
            wait_objects_for_debugging.emplace_back(objects[i]);
        }
    }

    const std::vector<KSynchronizationObject*>& GetWaitObjectsForDebugging() const {
        return wait_objects_for_debugging;
    }

private:
    friend class GlobalSchedulerContext;
    friend class KScheduler;
    friend class Process;

    void SetSchedulingStatus(ThreadState new_status);
    void AddSchedulingFlag(ThreadSchedFlags flag);
    void RemoveSchedulingFlag(ThreadSchedFlags flag);
    void SetCurrentPriority(u32 new_priority);

    Common::SpinLock context_guard{};
    ThreadContext32 context_32{};
    ThreadContext64 context_64{};
    std::shared_ptr<Common::Fiber> host_context{};

    ThreadState thread_state = ThreadState::Initialized;

    u64 thread_id = 0;

    VAddr entry_point = 0;
    VAddr stack_top = 0;
    std::atomic_int disable_count = 0;

    ThreadType type;

    /// Nominal thread priority, as set by the emulated application.
    /// The nominal priority is the thread priority without priority
    /// inheritance taken into account.
    u32 nominal_priority = 0;

    /// Current thread priority. This may change over the course of the
    /// thread's lifetime in order to facilitate priority inheritance.
    u32 current_priority = 0;

    u64 total_cpu_time_ticks = 0; ///< Total CPU running ticks.
    s64 schedule_count{};
    s64 last_scheduled_tick{};

    s32 processor_id = 0;

    VAddr tls_address = 0; ///< Virtual address of the Thread Local Storage of the thread
    u64 tpidr_el0 = 0;     ///< TPIDR_EL0 read/write system register.

    /// Process that owns this thread
    Process* owner_process;

    /// Objects that the thread is waiting on, in the same order as they were
    /// passed to WaitSynchronization. This is used for debugging only.
    std::vector<KSynchronizationObject*> wait_objects_for_debugging;

    KSynchronizationObject* signaling_object;
    ResultCode signaling_result{RESULT_SUCCESS};

    /// List of threads that are waiting for a mutex that is held by this thread.
    MutexWaitingThreads wait_mutex_threads;

    /// Thread that owns the lock that this thread is waiting for.
    std::shared_ptr<Thread> lock_owner;

    /// If waiting on a ConditionVariable, this is the ConditionVariable address
    VAddr condvar_wait_address = 0;
    bool is_waiting_on_condvar{};
    /// If waiting on a Mutex, this is the mutex address
    VAddr mutex_wait_address = 0;
    /// The handle used to wait for the mutex.
    Handle wait_handle = 0;

    /// If waiting for an AddressArbiter, this is the address being waited on.
    VAddr arb_wait_address{0};
    bool waiting_for_arbitration{};

    /// Handle used as userdata to reference this object when inserting into the CoreTiming queue.
    Handle global_handle = 0;

    Handle hle_time_event;

    KScheduler* scheduler = nullptr;

    std::array<QueueEntry, Core::Hardware::NUM_CPU_CORES> per_core_priority_queue_entry{};

    u32 ideal_core{0xFFFFFFFF};
    KAffinityMask affinity_mask{};

    s32 ideal_core_override = -1;
    u32 affinity_override_count = 0;

    u32 pausing_state = 0;
    bool is_running = false;
    bool is_cancellable = false;
    bool is_sync_cancelled = false;

    bool is_continuous_on_svc = false;

    bool will_be_terminated = false;
    bool is_phantom_mode = false;
    bool has_exited = false;

    bool was_running = false;

    bool signaled{};

    std::string name;
};

} // namespace Kernel
