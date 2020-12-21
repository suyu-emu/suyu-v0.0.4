// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <vector>

#include "common/assert.h"
#include "common/common_types.h"
#include "core/arm/exclusive_monitor.h"
#include "core/core.h"
#include "core/hle/kernel/address_arbiter.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/k_scheduler.h"
#include "core/hle/kernel/k_scoped_scheduler_lock_and_sleep.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/time_manager.h"
#include "core/hle/result.h"
#include "core/memory.h"

namespace Kernel {

// Wake up num_to_wake (or all) threads in a vector.
void AddressArbiter::WakeThreads(const std::vector<std::shared_ptr<Thread>>& waiting_threads,
                                 s32 num_to_wake) {
    // Only process up to 'target' threads, unless 'target' is <= 0, in which case process
    // them all.
    std::size_t last = waiting_threads.size();
    if (num_to_wake > 0) {
        last = std::min(last, static_cast<std::size_t>(num_to_wake));
    }

    // Signal the waiting threads.
    for (std::size_t i = 0; i < last; i++) {
        waiting_threads[i]->SetSynchronizationResults(nullptr, RESULT_SUCCESS);
        RemoveThread(waiting_threads[i]);
        waiting_threads[i]->WaitForArbitration(false);
        waiting_threads[i]->ResumeFromWait();
    }
}

AddressArbiter::AddressArbiter(Core::System& system) : system{system} {}
AddressArbiter::~AddressArbiter() = default;

ResultCode AddressArbiter::SignalToAddress(VAddr address, SignalType type, s32 value,
                                           s32 num_to_wake) {
    switch (type) {
    case SignalType::Signal:
        return SignalToAddressOnly(address, num_to_wake);
    case SignalType::IncrementAndSignalIfEqual:
        return IncrementAndSignalToAddressIfEqual(address, value, num_to_wake);
    case SignalType::ModifyByWaitingCountAndSignalIfEqual:
        return ModifyByWaitingCountAndSignalToAddressIfEqual(address, value, num_to_wake);
    default:
        return ERR_INVALID_ENUM_VALUE;
    }
}

ResultCode AddressArbiter::SignalToAddressOnly(VAddr address, s32 num_to_wake) {
    KScopedSchedulerLock lock(system.Kernel());
    const std::vector<std::shared_ptr<Thread>> waiting_threads =
        GetThreadsWaitingOnAddress(address);
    WakeThreads(waiting_threads, num_to_wake);
    return RESULT_SUCCESS;
}

ResultCode AddressArbiter::IncrementAndSignalToAddressIfEqual(VAddr address, s32 value,
                                                              s32 num_to_wake) {
    KScopedSchedulerLock lock(system.Kernel());
    auto& memory = system.Memory();

    // Ensure that we can write to the address.
    if (!memory.IsValidVirtualAddress(address)) {
        return ERR_INVALID_ADDRESS_STATE;
    }

    const std::size_t current_core = system.CurrentCoreIndex();
    auto& monitor = system.Monitor();
    u32 current_value;
    do {
        current_value = monitor.ExclusiveRead32(current_core, address);

        if (current_value != static_cast<u32>(value)) {
            return ERR_INVALID_STATE;
        }
        current_value++;
    } while (!monitor.ExclusiveWrite32(current_core, address, current_value));

    return SignalToAddressOnly(address, num_to_wake);
}

ResultCode AddressArbiter::ModifyByWaitingCountAndSignalToAddressIfEqual(VAddr address, s32 value,
                                                                         s32 num_to_wake) {
    KScopedSchedulerLock lock(system.Kernel());
    auto& memory = system.Memory();

    // Ensure that we can write to the address.
    if (!memory.IsValidVirtualAddress(address)) {
        return ERR_INVALID_ADDRESS_STATE;
    }

    // Get threads waiting on the address.
    const std::vector<std::shared_ptr<Thread>> waiting_threads =
        GetThreadsWaitingOnAddress(address);

    const std::size_t current_core = system.CurrentCoreIndex();
    auto& monitor = system.Monitor();
    s32 updated_value;
    do {
        updated_value = monitor.ExclusiveRead32(current_core, address);

        if (updated_value != value) {
            return ERR_INVALID_STATE;
        }
        // Determine the modified value depending on the waiting count.
        if (num_to_wake <= 0) {
            if (waiting_threads.empty()) {
                updated_value = value + 1;
            } else {
                updated_value = value - 1;
            }
        } else {
            if (waiting_threads.empty()) {
                updated_value = value + 1;
            } else if (waiting_threads.size() <= static_cast<u32>(num_to_wake)) {
                updated_value = value - 1;
            } else {
                updated_value = value;
            }
        }
    } while (!monitor.ExclusiveWrite32(current_core, address, updated_value));

    WakeThreads(waiting_threads, num_to_wake);
    return RESULT_SUCCESS;
}

ResultCode AddressArbiter::WaitForAddress(VAddr address, ArbitrationType type, s32 value,
                                          s64 timeout_ns) {
    switch (type) {
    case ArbitrationType::WaitIfLessThan:
        return WaitForAddressIfLessThan(address, value, timeout_ns, false);
    case ArbitrationType::DecrementAndWaitIfLessThan:
        return WaitForAddressIfLessThan(address, value, timeout_ns, true);
    case ArbitrationType::WaitIfEqual:
        return WaitForAddressIfEqual(address, value, timeout_ns);
    default:
        return ERR_INVALID_ENUM_VALUE;
    }
}

ResultCode AddressArbiter::WaitForAddressIfLessThan(VAddr address, s32 value, s64 timeout,
                                                    bool should_decrement) {
    auto& memory = system.Memory();
    auto& kernel = system.Kernel();
    Thread* current_thread = kernel.CurrentScheduler()->GetCurrentThread();

    Handle event_handle = InvalidHandle;
    {
        KScopedSchedulerLockAndSleep lock(kernel, event_handle, current_thread, timeout);

        if (current_thread->IsPendingTermination()) {
            lock.CancelSleep();
            return ERR_THREAD_TERMINATING;
        }

        // Ensure that we can read the address.
        if (!memory.IsValidVirtualAddress(address)) {
            lock.CancelSleep();
            return ERR_INVALID_ADDRESS_STATE;
        }

        s32 current_value = static_cast<s32>(memory.Read32(address));
        if (current_value >= value) {
            lock.CancelSleep();
            return ERR_INVALID_STATE;
        }

        current_thread->SetSynchronizationResults(nullptr, RESULT_TIMEOUT);

        s32 decrement_value;

        const std::size_t current_core = system.CurrentCoreIndex();
        auto& monitor = system.Monitor();
        do {
            current_value = static_cast<s32>(monitor.ExclusiveRead32(current_core, address));
            if (should_decrement) {
                decrement_value = current_value - 1;
            } else {
                decrement_value = current_value;
            }
        } while (
            !monitor.ExclusiveWrite32(current_core, address, static_cast<u32>(decrement_value)));

        // Short-circuit without rescheduling, if timeout is zero.
        if (timeout == 0) {
            lock.CancelSleep();
            return RESULT_TIMEOUT;
        }

        current_thread->SetArbiterWaitAddress(address);
        InsertThread(SharedFrom(current_thread));
        current_thread->SetStatus(ThreadStatus::WaitArb);
        current_thread->WaitForArbitration(true);
    }

    if (event_handle != InvalidHandle) {
        auto& time_manager = kernel.TimeManager();
        time_manager.UnscheduleTimeEvent(event_handle);
    }

    {
        KScopedSchedulerLock lock(kernel);
        if (current_thread->IsWaitingForArbitration()) {
            RemoveThread(SharedFrom(current_thread));
            current_thread->WaitForArbitration(false);
        }
    }

    return current_thread->GetSignalingResult();
}

ResultCode AddressArbiter::WaitForAddressIfEqual(VAddr address, s32 value, s64 timeout) {
    auto& memory = system.Memory();
    auto& kernel = system.Kernel();
    Thread* current_thread = kernel.CurrentScheduler()->GetCurrentThread();

    Handle event_handle = InvalidHandle;
    {
        KScopedSchedulerLockAndSleep lock(kernel, event_handle, current_thread, timeout);

        if (current_thread->IsPendingTermination()) {
            lock.CancelSleep();
            return ERR_THREAD_TERMINATING;
        }

        // Ensure that we can read the address.
        if (!memory.IsValidVirtualAddress(address)) {
            lock.CancelSleep();
            return ERR_INVALID_ADDRESS_STATE;
        }

        s32 current_value = static_cast<s32>(memory.Read32(address));
        if (current_value != value) {
            lock.CancelSleep();
            return ERR_INVALID_STATE;
        }

        // Short-circuit without rescheduling, if timeout is zero.
        if (timeout == 0) {
            lock.CancelSleep();
            return RESULT_TIMEOUT;
        }

        current_thread->SetSynchronizationResults(nullptr, RESULT_TIMEOUT);
        current_thread->SetArbiterWaitAddress(address);
        InsertThread(SharedFrom(current_thread));
        current_thread->SetStatus(ThreadStatus::WaitArb);
        current_thread->WaitForArbitration(true);
    }

    if (event_handle != InvalidHandle) {
        auto& time_manager = kernel.TimeManager();
        time_manager.UnscheduleTimeEvent(event_handle);
    }

    {
        KScopedSchedulerLock lock(kernel);
        if (current_thread->IsWaitingForArbitration()) {
            RemoveThread(SharedFrom(current_thread));
            current_thread->WaitForArbitration(false);
        }
    }

    return current_thread->GetSignalingResult();
}

void AddressArbiter::InsertThread(std::shared_ptr<Thread> thread) {
    const VAddr arb_addr = thread->GetArbiterWaitAddress();
    std::list<std::shared_ptr<Thread>>& thread_list = arb_threads[arb_addr];

    const auto iter =
        std::find_if(thread_list.cbegin(), thread_list.cend(), [&thread](const auto& entry) {
            return entry->GetPriority() >= thread->GetPriority();
        });

    if (iter == thread_list.cend()) {
        thread_list.push_back(std::move(thread));
    } else {
        thread_list.insert(iter, std::move(thread));
    }
}

void AddressArbiter::RemoveThread(std::shared_ptr<Thread> thread) {
    const VAddr arb_addr = thread->GetArbiterWaitAddress();
    std::list<std::shared_ptr<Thread>>& thread_list = arb_threads[arb_addr];

    const auto iter = std::find_if(thread_list.cbegin(), thread_list.cend(),
                                   [&thread](const auto& entry) { return thread == entry; });

    if (iter != thread_list.cend()) {
        thread_list.erase(iter);
    }
}

std::vector<std::shared_ptr<Thread>> AddressArbiter::GetThreadsWaitingOnAddress(
    VAddr address) const {
    const auto iter = arb_threads.find(address);
    if (iter == arb_threads.cend()) {
        return {};
    }

    const std::list<std::shared_ptr<Thread>>& thread_list = iter->second;
    return {thread_list.cbegin(), thread_list.cend()};
}
} // namespace Kernel
