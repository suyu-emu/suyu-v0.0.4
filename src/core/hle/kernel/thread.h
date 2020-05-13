// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "common/common_types.h"
#include "common/spin_lock.h"
#include "core/arm/arm_interface.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/synchronization_object.h"
#include "core/hle/result.h"

namespace Common {
class Fiber;
}

namespace Core {
class ARM_Interface;
class System;
} // namespace Core

namespace Kernel {

class GlobalScheduler;
class KernelCore;
class Process;
class Scheduler;

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

enum class ThreadStatus {
    Running,      ///< Currently running
    Ready,        ///< Ready to run
    Paused,       ///< Paused by SetThreadActivity or debug
    WaitHLEEvent, ///< Waiting for hle event to finish
    WaitSleep,    ///< Waiting due to a SleepThread SVC
    WaitIPC,      ///< Waiting for the reply from an IPC request
    WaitSynch,    ///< Waiting due to WaitSynchronization
    WaitMutex,    ///< Waiting due to an ArbitrateLock svc
    WaitCondVar,  ///< Waiting due to an WaitProcessWideKey svc
    WaitArb,      ///< Waiting due to a SignalToAddress/WaitForAddress svc
    Dormant,      ///< Created but not yet made ready
    Dead          ///< Run to completion, or forcefully terminated
};

enum class ThreadWakeupReason {
    Signal, // The thread was woken up by WakeupAllWaitingThreads due to an object signal.
    Timeout // The thread was woken up due to a wait timeout.
};

enum class ThreadActivity : u32 {
    Normal = 0,
    Paused = 1,
};

enum class ThreadSchedStatus : u32 {
    None = 0,
    Paused = 1,
    Runnable = 2,
    Exited = 3,
};

enum class ThreadSchedFlags : u32 {
    ProcessPauseFlag = 1 << 4,
    ThreadPauseFlag = 1 << 5,
    ProcessDebugPauseFlag = 1 << 6,
    KernelInitPauseFlag = 1 << 8,
};

enum class ThreadSchedMasks : u32 {
    LowMask = 0x000f,
    HighMask = 0xfff0,
    ForcePauseMask = 0x0070,
};

class Thread final : public SynchronizationObject {
public:
    explicit Thread(KernelCore& kernel);
    ~Thread() override;

    using MutexWaitingThreads = std::vector<std::shared_ptr<Thread>>;

    using ThreadContext32 = Core::ARM_Interface::ThreadContext32;
    using ThreadContext64 = Core::ARM_Interface::ThreadContext64;

    using ThreadSynchronizationObjects = std::vector<std::shared_ptr<SynchronizationObject>>;

    using HLECallback = std::function<bool(std::shared_ptr<Thread> thread)>;

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

    bool ShouldWait(const Thread* thread) const override;
    void Acquire(Thread* thread) override;
    bool IsSignaled() const override;

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
    void ResumeFromWait();

    void OnWakeUp();

    ResultCode Start();

    /// Cancels a waiting operation that this thread may or may not be within.
    ///
    /// When the thread is within a waiting state, this will set the thread's
    /// waiting result to signal a canceled wait. The function will then resume
    /// this thread.
    ///
    void CancelWait();

    void SetSynchronizationResults(SynchronizationObject* object, ResultCode result);

    Core::ARM_Interface& ArmInterface();

    const Core::ARM_Interface& ArmInterface() const;

    SynchronizationObject* GetSignalingObject() const {
        return signaling_object;
    }

    ResultCode GetSignalingResult() const {
        return signaling_result;
    }

    /**
     * Retrieves the index that this particular object occupies in the list of objects
     * that the thread passed to WaitSynchronization, starting the search from the last element.
     *
     * It is used to set the output index of WaitSynchronization when the thread is awakened.
     *
     * When a thread wakes up due to an object signal, the kernel will use the index of the last
     * matching object in the wait objects list in case of having multiple instances of the same
     * object in the list.
     *
     * @param object Object to query the index of.
     */
    s32 GetSynchronizationObjectIndex(std::shared_ptr<SynchronizationObject> object) const;

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

    ThreadStatus GetStatus() const {
        return status;
    }

    void SetStatus(ThreadStatus new_status);

    u64 GetLastRunningTicks() const {
        return last_running_ticks;
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

    void SetProcessorID(s32 new_core) {
        processor_id = new_core;
    }

    Process* GetOwnerProcess() {
        return owner_process;
    }

    const Process* GetOwnerProcess() const {
        return owner_process;
    }

    const ThreadSynchronizationObjects& GetSynchronizationObjects() const {
        return *wait_objects;
    }

    void SetSynchronizationObjects(ThreadSynchronizationObjects* objects) {
        wait_objects = objects;
    }

    void ClearSynchronizationObjects() {
        for (const auto& waiting_object : *wait_objects) {
            waiting_object->RemoveWaitingThread(SharedFrom(this));
        }
        wait_objects->clear();
    }

    /// Determines whether all the objects this thread is waiting on are ready.
    bool AllSynchronizationObjectsReady() const;

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

    bool HasHLECallback() const {
        return hle_callback != nullptr;
    }

    void SetHLECallback(HLECallback callback) {
        hle_callback = std::move(callback);
    }

    void SetHLETimeEvent(Handle time_event) {
        hle_time_event = time_event;
    }

    void SetHLESyncObject(SynchronizationObject* object) {
        hle_object = object;
    }

    Handle GetHLETimeEvent() const {
        return hle_time_event;
    }

    SynchronizationObject* GetHLESyncObject() const {
        return hle_object;
    }

    void InvalidateHLECallback() {
        SetHLECallback(nullptr);
    }

    bool InvokeHLECallback(std::shared_ptr<Thread> thread);

    u32 GetIdealCore() const {
        return ideal_core;
    }

    u64 GetAffinityMask() const {
        return affinity_mask;
    }

    ResultCode SetActivity(ThreadActivity value);

    /// Sleeps this thread for the given amount of nanoseconds.
    ResultCode Sleep(s64 nanoseconds);

    /// Yields this thread without rebalancing loads.
    std::pair<ResultCode, bool> YieldSimple();

    /// Yields this thread and does a load rebalancing.
    std::pair<ResultCode, bool> YieldAndBalanceLoad();

    /// Yields this thread and if the core is left idle, loads are rebalanced
    std::pair<ResultCode, bool> YieldAndWaitForLoadBalancing();

    void IncrementYieldCount() {
        yield_count++;
    }

    u64 GetYieldCount() const {
        return yield_count;
    }

    ThreadSchedStatus GetSchedulingStatus() const {
        return static_cast<ThreadSchedStatus>(scheduling_state &
                                              static_cast<u32>(ThreadSchedMasks::LowMask));
    }

    bool IsRunnable() const {
        return scheduling_state == static_cast<u32>(ThreadSchedStatus::Runnable);
    }

    bool IsRunning() const {
        return is_running;
    }

    void SetIsRunning(bool value) {
        is_running = value;
    }

    bool IsSyncCancelled() const {
        return is_sync_cancelled;
    }

    void SetSyncCancelled(bool value) {
        is_sync_cancelled = value;
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

    bool IsWaitingSync() const {
        return is_waiting_on_sync;
    }

    void SetWaitingSync(bool is_waiting) {
        is_waiting_on_sync = is_waiting;
    }

    bool IsPendingTermination() const {
        return will_be_terminated || GetSchedulingStatus() == ThreadSchedStatus::Exited;
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

private:
    friend class GlobalScheduler;
    friend class Scheduler;

    void SetSchedulingStatus(ThreadSchedStatus new_status);
    void AddSchedulingFlag(ThreadSchedFlags flag);
    void RemoveSchedulingFlag(ThreadSchedFlags flag);

    void SetCurrentPriority(u32 new_priority);

    void AdjustSchedulingOnAffinity(u64 old_affinity_mask, s32 old_core);

    Common::SpinLock context_guard{};
    ThreadContext32 context_32{};
    ThreadContext64 context_64{};
    std::unique_ptr<Core::ARM_Interface> arm_interface{};
    std::shared_ptr<Common::Fiber> host_context{};

    u64 thread_id = 0;

    ThreadStatus status = ThreadStatus::Dormant;

    VAddr entry_point = 0;
    VAddr stack_top = 0;

    ThreadType type;

    /// Nominal thread priority, as set by the emulated application.
    /// The nominal priority is the thread priority without priority
    /// inheritance taken into account.
    u32 nominal_priority = 0;

    /// Current thread priority. This may change over the course of the
    /// thread's lifetime in order to facilitate priority inheritance.
    u32 current_priority = 0;

    u64 total_cpu_time_ticks = 0; ///< Total CPU running ticks.
    u64 last_running_ticks = 0;   ///< CPU tick when thread was last running
    u64 yield_count = 0;          ///< Number of redundant yields carried by this thread.
                                  ///< a redundant yield is one where no scheduling is changed

    s32 processor_id = 0;

    VAddr tls_address = 0; ///< Virtual address of the Thread Local Storage of the thread
    u64 tpidr_el0 = 0;     ///< TPIDR_EL0 read/write system register.

    /// Process that owns this thread
    Process* owner_process;

    /// Objects that the thread is waiting on, in the same order as they were
    /// passed to WaitSynchronization.
    ThreadSynchronizationObjects* wait_objects;

    SynchronizationObject* signaling_object;
    ResultCode signaling_result{RESULT_SUCCESS};

    /// List of threads that are waiting for a mutex that is held by this thread.
    MutexWaitingThreads wait_mutex_threads;

    /// Thread that owns the lock that this thread is waiting for.
    std::shared_ptr<Thread> lock_owner;

    /// If waiting on a ConditionVariable, this is the ConditionVariable address
    VAddr condvar_wait_address = 0;
    /// If waiting on a Mutex, this is the mutex address
    VAddr mutex_wait_address = 0;
    /// The handle used to wait for the mutex.
    Handle wait_handle = 0;

    /// If waiting for an AddressArbiter, this is the address being waited on.
    VAddr arb_wait_address{0};
    bool waiting_for_arbitration{};

    /// Handle used as userdata to reference this object when inserting into the CoreTiming queue.
    Handle global_handle = 0;

    /// Callback for HLE Events
    HLECallback hle_callback;
    Handle hle_time_event;
    SynchronizationObject* hle_object;

    Scheduler* scheduler = nullptr;

    u32 ideal_core{0xFFFFFFFF};
    u64 affinity_mask{0x1};

    s32 ideal_core_override = -1;
    u64 affinity_mask_override = 0x1;
    u32 affinity_override_count = 0;

    u32 scheduling_state = 0;
    u32 pausing_state = 0;
    bool is_running = false;
    bool is_waiting_on_sync = false;
    bool is_sync_cancelled = false;

    bool is_continuous_on_svc = false;

    bool will_be_terminated = false;
    bool is_phantom_mode = false;
    bool has_exited = false;

    bool was_running = false;

    std::string name;
};

/**
 * Gets the current thread
 */
Thread* GetCurrentThread();

} // namespace Kernel
