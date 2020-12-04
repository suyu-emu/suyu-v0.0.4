// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <utility>
#include <vector>

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/k_scheduler.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/mutex.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/result.h"
#include "core/memory.h"

namespace Kernel {

/// Returns the number of threads that are waiting for a mutex, and the highest priority one among
/// those.
static std::pair<std::shared_ptr<Thread>, u32> GetHighestPriorityMutexWaitingThread(
    const std::shared_ptr<Thread>& current_thread, VAddr mutex_addr) {

    std::shared_ptr<Thread> highest_priority_thread;
    u32 num_waiters = 0;

    for (const auto& thread : current_thread->GetMutexWaitingThreads()) {
        if (thread->GetMutexWaitAddress() != mutex_addr)
            continue;

        ++num_waiters;
        if (highest_priority_thread == nullptr ||
            thread->GetPriority() < highest_priority_thread->GetPriority()) {
            highest_priority_thread = thread;
        }
    }

    return {highest_priority_thread, num_waiters};
}

/// Update the mutex owner field of all threads waiting on the mutex to point to the new owner.
static void TransferMutexOwnership(VAddr mutex_addr, std::shared_ptr<Thread> current_thread,
                                   std::shared_ptr<Thread> new_owner) {
    current_thread->RemoveMutexWaiter(new_owner);
    const auto threads = current_thread->GetMutexWaitingThreads();
    for (const auto& thread : threads) {
        if (thread->GetMutexWaitAddress() != mutex_addr)
            continue;

        ASSERT(thread->GetLockOwner() == current_thread.get());
        current_thread->RemoveMutexWaiter(thread);
        if (new_owner != thread)
            new_owner->AddMutexWaiter(thread);
    }
}

Mutex::Mutex(Core::System& system) : system{system} {}
Mutex::~Mutex() = default;

ResultCode Mutex::TryAcquire(VAddr address, Handle holding_thread_handle,
                             Handle requesting_thread_handle) {
    // The mutex address must be 4-byte aligned
    if ((address % sizeof(u32)) != 0) {
        LOG_ERROR(Kernel, "Address is not 4-byte aligned! address={:016X}", address);
        return ERR_INVALID_ADDRESS;
    }

    auto& kernel = system.Kernel();
    std::shared_ptr<Thread> current_thread =
        SharedFrom(kernel.CurrentScheduler()->GetCurrentThread());
    {
        KScopedSchedulerLock lock(kernel);
        // The mutex address must be 4-byte aligned
        if ((address % sizeof(u32)) != 0) {
            return ERR_INVALID_ADDRESS;
        }

        const auto& handle_table = kernel.CurrentProcess()->GetHandleTable();
        std::shared_ptr<Thread> holding_thread = handle_table.Get<Thread>(holding_thread_handle);
        std::shared_ptr<Thread> requesting_thread =
            handle_table.Get<Thread>(requesting_thread_handle);

        // TODO(Subv): It is currently unknown if it is possible to lock a mutex in behalf of
        // another thread.
        ASSERT(requesting_thread == current_thread);

        current_thread->SetSynchronizationResults(nullptr, RESULT_SUCCESS);

        const u32 addr_value = system.Memory().Read32(address);

        // If the mutex isn't being held, just return success.
        if (addr_value != (holding_thread_handle | Mutex::MutexHasWaitersFlag)) {
            return RESULT_SUCCESS;
        }

        if (holding_thread == nullptr) {
            return ERR_INVALID_HANDLE;
        }

        // Wait until the mutex is released
        current_thread->SetMutexWaitAddress(address);
        current_thread->SetWaitHandle(requesting_thread_handle);

        current_thread->SetStatus(ThreadStatus::WaitMutex);

        // Update the lock holder thread's priority to prevent priority inversion.
        holding_thread->AddMutexWaiter(current_thread);
    }

    {
        KScopedSchedulerLock lock(kernel);
        auto* owner = current_thread->GetLockOwner();
        if (owner != nullptr) {
            owner->RemoveMutexWaiter(current_thread);
        }
    }
    return current_thread->GetSignalingResult();
}

std::pair<ResultCode, std::shared_ptr<Thread>> Mutex::Unlock(std::shared_ptr<Thread> owner,
                                                             VAddr address) {
    // The mutex address must be 4-byte aligned
    if ((address % sizeof(u32)) != 0) {
        LOG_ERROR(Kernel, "Address is not 4-byte aligned! address={:016X}", address);
        return {ERR_INVALID_ADDRESS, nullptr};
    }

    auto [new_owner, num_waiters] = GetHighestPriorityMutexWaitingThread(owner, address);
    if (new_owner == nullptr) {
        system.Memory().Write32(address, 0);
        return {RESULT_SUCCESS, nullptr};
    }
    // Transfer the ownership of the mutex from the previous owner to the new one.
    TransferMutexOwnership(address, owner, new_owner);
    u32 mutex_value = new_owner->GetWaitHandle();
    if (num_waiters >= 2) {
        // Notify the guest that there are still some threads waiting for the mutex
        mutex_value |= Mutex::MutexHasWaitersFlag;
    }
    new_owner->SetSynchronizationResults(nullptr, RESULT_SUCCESS);
    new_owner->SetLockOwner(nullptr);
    new_owner->ResumeFromWait();

    system.Memory().Write32(address, mutex_value);
    return {RESULT_SUCCESS, new_owner};
}

ResultCode Mutex::Release(VAddr address) {
    auto& kernel = system.Kernel();
    KScopedSchedulerLock lock(kernel);

    std::shared_ptr<Thread> current_thread =
        SharedFrom(kernel.CurrentScheduler()->GetCurrentThread());

    auto [result, new_owner] = Unlock(current_thread, address);

    if (result != RESULT_SUCCESS && new_owner != nullptr) {
        new_owner->SetSynchronizationResults(nullptr, result);
    }

    return result;
}

} // namespace Kernel
