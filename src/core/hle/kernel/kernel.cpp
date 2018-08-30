// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <utility>

#include "common/assert.h"
#include "common/logging/log.h"

#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/timer.h"
#include "core/hle/lock.h"
#include "core/hle/result.h"

namespace Kernel {

/**
 * Callback that will wake up the thread it was scheduled for
 * @param thread_handle The handle of the thread that's been awoken
 * @param cycles_late The number of CPU cycles that have passed since the desired wakeup time
 */
static void ThreadWakeupCallback(u64 thread_handle, [[maybe_unused]] int cycles_late) {
    const auto proper_handle = static_cast<Handle>(thread_handle);
    auto& system = Core::System::GetInstance();

    // Lock the global kernel mutex when we enter the kernel HLE.
    std::lock_guard<std::recursive_mutex> lock(HLE::g_hle_lock);

    SharedPtr<Thread> thread =
        system.Kernel().RetrieveThreadFromWakeupCallbackHandleTable(proper_handle);
    if (thread == nullptr) {
        LOG_CRITICAL(Kernel, "Callback fired for invalid thread {:08X}", proper_handle);
        return;
    }

    bool resume = true;

    if (thread->status == ThreadStatus::WaitSynchAny ||
        thread->status == ThreadStatus::WaitSynchAll ||
        thread->status == ThreadStatus::WaitHLEEvent) {
        // Remove the thread from each of its waiting objects' waitlists
        for (auto& object : thread->wait_objects) {
            object->RemoveWaitingThread(thread.get());
        }
        thread->wait_objects.clear();

        // Invoke the wakeup callback before clearing the wait objects
        if (thread->wakeup_callback) {
            resume = thread->wakeup_callback(ThreadWakeupReason::Timeout, thread, nullptr, 0);
        }
    }

    if (thread->mutex_wait_address != 0 || thread->condvar_wait_address != 0 ||
        thread->wait_handle) {
        ASSERT(thread->status == ThreadStatus::WaitMutex);
        thread->mutex_wait_address = 0;
        thread->condvar_wait_address = 0;
        thread->wait_handle = 0;

        auto lock_owner = thread->lock_owner;
        // Threads waking up by timeout from WaitProcessWideKey do not perform priority inheritance
        // and don't have a lock owner unless SignalProcessWideKey was called first and the thread
        // wasn't awakened due to the mutex already being acquired.
        if (lock_owner) {
            lock_owner->RemoveMutexWaiter(thread);
        }
    }

    if (thread->arb_wait_address != 0) {
        ASSERT(thread->status == ThreadStatus::WaitArb);
        thread->arb_wait_address = 0;
    }

    if (resume) {
        thread->ResumeFromWait();
    }
}

/// The timer callback event, called when a timer is fired
static void TimerCallback(u64 timer_handle, int cycles_late) {
    const auto proper_handle = static_cast<Handle>(timer_handle);
    auto& system = Core::System::GetInstance();
    SharedPtr<Timer> timer = system.Kernel().RetrieveTimerFromCallbackHandleTable(proper_handle);

    if (timer == nullptr) {
        LOG_CRITICAL(Kernel, "Callback fired for invalid timer {:016X}", timer_handle);
        return;
    }

    timer->Signal(cycles_late);
}

struct KernelCore::Impl {
    void Initialize(KernelCore& kernel) {
        Shutdown();

        InitializeResourceLimits(kernel);
        InitializeThreads();
        InitializeTimers();
    }

    void Shutdown() {
        next_object_id = 0;
        next_process_id = 10;
        next_thread_id = 1;

        process_list.clear();

        handle_table.Clear();
        resource_limits.fill(nullptr);

        thread_wakeup_callback_handle_table.Clear();
        thread_wakeup_event_type = nullptr;

        timer_callback_handle_table.Clear();
        timer_callback_event_type = nullptr;
    }

    void InitializeResourceLimits(KernelCore& kernel) {
        // Create the four resource limits that the system uses
        // Create the APPLICATION resource limit
        SharedPtr<ResourceLimit> resource_limit = ResourceLimit::Create(kernel, "Applications");
        resource_limit->max_priority = 0x18;
        resource_limit->max_commit = 0x4000000;
        resource_limit->max_threads = 0x20;
        resource_limit->max_events = 0x20;
        resource_limit->max_mutexes = 0x20;
        resource_limit->max_semaphores = 0x8;
        resource_limit->max_timers = 0x8;
        resource_limit->max_shared_mems = 0x10;
        resource_limit->max_address_arbiters = 0x2;
        resource_limit->max_cpu_time = 0x1E;
        resource_limits[static_cast<u8>(ResourceLimitCategory::APPLICATION)] = resource_limit;

        // Create the SYS_APPLET resource limit
        resource_limit = ResourceLimit::Create(kernel, "System Applets");
        resource_limit->max_priority = 0x4;
        resource_limit->max_commit = 0x5E00000;
        resource_limit->max_threads = 0x1D;
        resource_limit->max_events = 0xB;
        resource_limit->max_mutexes = 0x8;
        resource_limit->max_semaphores = 0x4;
        resource_limit->max_timers = 0x4;
        resource_limit->max_shared_mems = 0x8;
        resource_limit->max_address_arbiters = 0x3;
        resource_limit->max_cpu_time = 0x2710;
        resource_limits[static_cast<u8>(ResourceLimitCategory::SYS_APPLET)] = resource_limit;

        // Create the LIB_APPLET resource limit
        resource_limit = ResourceLimit::Create(kernel, "Library Applets");
        resource_limit->max_priority = 0x4;
        resource_limit->max_commit = 0x600000;
        resource_limit->max_threads = 0xE;
        resource_limit->max_events = 0x8;
        resource_limit->max_mutexes = 0x8;
        resource_limit->max_semaphores = 0x4;
        resource_limit->max_timers = 0x4;
        resource_limit->max_shared_mems = 0x8;
        resource_limit->max_address_arbiters = 0x1;
        resource_limit->max_cpu_time = 0x2710;
        resource_limits[static_cast<u8>(ResourceLimitCategory::LIB_APPLET)] = resource_limit;

        // Create the OTHER resource limit
        resource_limit = ResourceLimit::Create(kernel, "Others");
        resource_limit->max_priority = 0x4;
        resource_limit->max_commit = 0x2180000;
        resource_limit->max_threads = 0xE1;
        resource_limit->max_events = 0x108;
        resource_limit->max_mutexes = 0x25;
        resource_limit->max_semaphores = 0x43;
        resource_limit->max_timers = 0x2C;
        resource_limit->max_shared_mems = 0x1F;
        resource_limit->max_address_arbiters = 0x2D;
        resource_limit->max_cpu_time = 0x3E8;
        resource_limits[static_cast<u8>(ResourceLimitCategory::OTHER)] = resource_limit;
    }

    void InitializeThreads() {
        thread_wakeup_event_type =
            CoreTiming::RegisterEvent("ThreadWakeupCallback", ThreadWakeupCallback);
    }

    void InitializeTimers() {
        timer_callback_handle_table.Clear();
        timer_callback_event_type = CoreTiming::RegisterEvent("TimerCallback", TimerCallback);
    }

    std::atomic<u32> next_object_id{0};
    // TODO(Subv): Start the process ids from 10 for now, as lower PIDs are
    // reserved for low-level services
    std::atomic<u32> next_process_id{10};
    std::atomic<u32> next_thread_id{1};

    // Lists all processes that exist in the current session.
    std::vector<SharedPtr<Process>> process_list;

    Kernel::HandleTable handle_table;
    std::array<SharedPtr<ResourceLimit>, 4> resource_limits;

    /// The event type of the generic timer callback event
    CoreTiming::EventType* timer_callback_event_type = nullptr;
    // TODO(yuriks): This can be removed if Timer objects are explicitly pooled in the future,
    // allowing us to simply use a pool index or similar.
    Kernel::HandleTable timer_callback_handle_table;

    CoreTiming::EventType* thread_wakeup_event_type = nullptr;
    // TODO(yuriks): This can be removed if Thread objects are explicitly pooled in the future,
    // allowing us to simply use a pool index or similar.
    Kernel::HandleTable thread_wakeup_callback_handle_table;
};

KernelCore::KernelCore() : impl{std::make_unique<Impl>()} {}
KernelCore::~KernelCore() {
    Shutdown();
}

void KernelCore::Initialize() {
    impl->Initialize(*this);
}

void KernelCore::Shutdown() {
    impl->Shutdown();
}

Kernel::HandleTable& KernelCore::HandleTable() {
    return impl->handle_table;
}

const Kernel::HandleTable& KernelCore::HandleTable() const {
    return impl->handle_table;
}

SharedPtr<ResourceLimit> KernelCore::ResourceLimitForCategory(
    ResourceLimitCategory category) const {
    return impl->resource_limits.at(static_cast<std::size_t>(category));
}

SharedPtr<Thread> KernelCore::RetrieveThreadFromWakeupCallbackHandleTable(Handle handle) const {
    return impl->thread_wakeup_callback_handle_table.Get<Thread>(handle);
}

SharedPtr<Timer> KernelCore::RetrieveTimerFromCallbackHandleTable(Handle handle) const {
    return impl->timer_callback_handle_table.Get<Timer>(handle);
}

void KernelCore::AppendNewProcess(SharedPtr<Process> process) {
    impl->process_list.push_back(std::move(process));
}

u32 KernelCore::CreateNewObjectID() {
    return impl->next_object_id++;
}

u32 KernelCore::CreateNewThreadID() {
    return impl->next_thread_id++;
}

u32 KernelCore::CreateNewProcessID() {
    return impl->next_process_id++;
}

ResultVal<Handle> KernelCore::CreateTimerCallbackHandle(const SharedPtr<Timer>& timer) {
    return impl->timer_callback_handle_table.Create(timer);
}

CoreTiming::EventType* KernelCore::ThreadWakeupCallbackEventType() const {
    return impl->thread_wakeup_event_type;
}

CoreTiming::EventType* KernelCore::TimerCallbackEventType() const {
    return impl->timer_callback_event_type;
}

Kernel::HandleTable& KernelCore::ThreadWakeupCallbackHandleTable() {
    return impl->thread_wakeup_callback_handle_table;
}

const Kernel::HandleTable& KernelCore::ThreadWakeupCallbackHandleTable() const {
    return impl->thread_wakeup_callback_handle_table;
}

} // namespace Kernel
