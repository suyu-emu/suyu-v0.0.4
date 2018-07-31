// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <map>
#include <vector>
#include <boost/range/algorithm_ext/erase.hpp>
#include "common/assert.h"
#include "core/core.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/mutex.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {

/// Returns the number of threads that are waiting for a mutex, and the highest priority one among
/// those.
static std::pair<SharedPtr<Thread>, u32> GetHighestPriorityMutexWaitingThread(
    const SharedPtr<Thread>& current_thread, VAddr mutex_addr) {

    SharedPtr<Thread> highest_priority_thread;
    u32 num_waiters = 0;

    for (auto& thread : current_thread->wait_mutex_threads) {
        if (thread->mutex_wait_address != mutex_addr)
            continue;

        ASSERT(thread->status == ThreadStatus::WaitMutex);

        ++num_waiters;
        if (highest_priority_thread == nullptr ||
            thread->GetPriority() < highest_priority_thread->GetPriority()) {
            highest_priority_thread = thread;
        }
    }

    return {highest_priority_thread, num_waiters};
}

/// Update the mutex owner field of all threads waiting on the mutex to point to the new owner.
static void TransferMutexOwnership(VAddr mutex_addr, SharedPtr<Thread> current_thread,
                                   SharedPtr<Thread> new_owner) {
    auto threads = current_thread->wait_mutex_threads;
    for (auto& thread : threads) {
        if (thread->mutex_wait_address != mutex_addr)
            continue;

        ASSERT(thread->lock_owner == current_thread);
        current_thread->RemoveMutexWaiter(thread);
        if (new_owner != thread)
            new_owner->AddMutexWaiter(thread);
    }
}

ResultCode Mutex::TryAcquire(VAddr address, Handle holding_thread_handle,
                             Handle requesting_thread_handle) {
    // The mutex address must be 4-byte aligned
    if ((address % sizeof(u32)) != 0) {
        return ResultCode(ErrorModule::Kernel, ErrCodes::InvalidAddress);
    }

    SharedPtr<Thread> holding_thread = g_handle_table.Get<Thread>(holding_thread_handle);
    SharedPtr<Thread> requesting_thread = g_handle_table.Get<Thread>(requesting_thread_handle);

    // TODO(Subv): It is currently unknown if it is possible to lock a mutex in behalf of another
    // thread.
    ASSERT(requesting_thread == GetCurrentThread());

    u32 addr_value = Memory::Read32(address);

    // If the mutex isn't being held, just return success.
    if (addr_value != (holding_thread_handle | Mutex::MutexHasWaitersFlag)) {
        return RESULT_SUCCESS;
    }

    if (holding_thread == nullptr)
        return ERR_INVALID_HANDLE;

    // Wait until the mutex is released
    GetCurrentThread()->mutex_wait_address = address;
    GetCurrentThread()->wait_handle = requesting_thread_handle;

    GetCurrentThread()->status = ThreadStatus::WaitMutex;
    GetCurrentThread()->wakeup_callback = nullptr;

    // Update the lock holder thread's priority to prevent priority inversion.
    holding_thread->AddMutexWaiter(GetCurrentThread());

    Core::System::GetInstance().PrepareReschedule();

    return RESULT_SUCCESS;
}

ResultCode Mutex::Release(VAddr address) {
    // The mutex address must be 4-byte aligned
    if ((address % sizeof(u32)) != 0) {
        return ResultCode(ErrorModule::Kernel, ErrCodes::InvalidAddress);
    }

    auto [thread, num_waiters] = GetHighestPriorityMutexWaitingThread(GetCurrentThread(), address);

    // There are no more threads waiting for the mutex, release it completely.
    if (thread == nullptr) {
        Memory::Write32(address, 0);
        return RESULT_SUCCESS;
    }

    // Transfer the ownership of the mutex from the previous owner to the new one.
    TransferMutexOwnership(address, GetCurrentThread(), thread);

    u32 mutex_value = thread->wait_handle;

    if (num_waiters >= 2) {
        // Notify the guest that there are still some threads waiting for the mutex
        mutex_value |= Mutex::MutexHasWaitersFlag;
    }

    // Grant the mutex to the next waiting thread and resume it.
    Memory::Write32(address, mutex_value);

    ASSERT(thread->status == ThreadStatus::WaitMutex);
    thread->ResumeFromWait();

    thread->lock_owner = nullptr;
    thread->condvar_wait_address = 0;
    thread->mutex_wait_address = 0;
    thread->wait_handle = 0;

    return RESULT_SUCCESS;
}
} // namespace Kernel
