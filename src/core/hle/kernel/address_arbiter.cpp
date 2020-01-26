// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <vector>

#include "common/assert.h"
#include "common/common_types.h"
#include "core/core.h"
#include "core/hle/kernel/address_arbiter.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/scheduler.h"
#include "core/hle/kernel/thread.h"
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
        ASSERT(waiting_threads[i]->GetStatus() == ThreadStatus::WaitArb);
        waiting_threads[i]->SetWaitSynchronizationResult(RESULT_SUCCESS);
        RemoveThread(waiting_threads[i]);
        waiting_threads[i]->SetArbiterWaitAddress(0);
        waiting_threads[i]->ResumeFromWait();
        system.PrepareReschedule(waiting_threads[i]->GetProcessorID());
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
    const std::vector<std::shared_ptr<Thread>> waiting_threads =
        GetThreadsWaitingOnAddress(address);
    WakeThreads(waiting_threads, num_to_wake);
    return RESULT_SUCCESS;
}

ResultCode AddressArbiter::IncrementAndSignalToAddressIfEqual(VAddr address, s32 value,
                                                              s32 num_to_wake) {
    auto& memory = system.Memory();

    // Ensure that we can write to the address.
    if (!memory.IsValidVirtualAddress(address)) {
        return ERR_INVALID_ADDRESS_STATE;
    }

    if (static_cast<s32>(memory.Read32(address)) != value) {
        return ERR_INVALID_STATE;
    }

    memory.Write32(address, static_cast<u32>(value + 1));
    return SignalToAddressOnly(address, num_to_wake);
}

ResultCode AddressArbiter::ModifyByWaitingCountAndSignalToAddressIfEqual(VAddr address, s32 value,
                                                                         s32 num_to_wake) {
    auto& memory = system.Memory();

    // Ensure that we can write to the address.
    if (!memory.IsValidVirtualAddress(address)) {
        return ERR_INVALID_ADDRESS_STATE;
    }

    // Get threads waiting on the address.
    const std::vector<std::shared_ptr<Thread>> waiting_threads =
        GetThreadsWaitingOnAddress(address);

    // Determine the modified value depending on the waiting count.
    s32 updated_value;
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

    if (static_cast<s32>(memory.Read32(address)) != value) {
        return ERR_INVALID_STATE;
    }

    memory.Write32(address, static_cast<u32>(updated_value));
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

    // Ensure that we can read the address.
    if (!memory.IsValidVirtualAddress(address)) {
        return ERR_INVALID_ADDRESS_STATE;
    }

    const s32 cur_value = static_cast<s32>(memory.Read32(address));
    if (cur_value >= value) {
        return ERR_INVALID_STATE;
    }

    if (should_decrement) {
        memory.Write32(address, static_cast<u32>(cur_value - 1));
    }

    // Short-circuit without rescheduling, if timeout is zero.
    if (timeout == 0) {
        return RESULT_TIMEOUT;
    }

    return WaitForAddressImpl(address, timeout);
}

ResultCode AddressArbiter::WaitForAddressIfEqual(VAddr address, s32 value, s64 timeout) {
    auto& memory = system.Memory();

    // Ensure that we can read the address.
    if (!memory.IsValidVirtualAddress(address)) {
        return ERR_INVALID_ADDRESS_STATE;
    }

    // Only wait for the address if equal.
    if (static_cast<s32>(memory.Read32(address)) != value) {
        return ERR_INVALID_STATE;
    }

    // Short-circuit without rescheduling if timeout is zero.
    if (timeout == 0) {
        return RESULT_TIMEOUT;
    }

    return WaitForAddressImpl(address, timeout);
}

ResultCode AddressArbiter::WaitForAddressImpl(VAddr address, s64 timeout) {
    Thread* current_thread = system.CurrentScheduler().GetCurrentThread();
    current_thread->SetArbiterWaitAddress(address);
    InsertThread(SharedFrom(current_thread));
    current_thread->SetStatus(ThreadStatus::WaitArb);
    current_thread->InvalidateWakeupCallback();
    current_thread->WakeAfterDelay(timeout);

    system.PrepareReschedule(current_thread->GetProcessorID());
    return RESULT_TIMEOUT;
}

void AddressArbiter::HandleWakeupThread(std::shared_ptr<Thread> thread) {
    ASSERT(thread->GetStatus() == ThreadStatus::WaitArb);
    RemoveThread(thread);
    thread->SetArbiterWaitAddress(0);
}

void AddressArbiter::InsertThread(std::shared_ptr<Thread> thread) {
    const VAddr arb_addr = thread->GetArbiterWaitAddress();
    std::list<std::shared_ptr<Thread>>& thread_list = arb_threads[arb_addr];
    auto it = thread_list.begin();
    while (it != thread_list.end()) {
        const std::shared_ptr<Thread>& current_thread = *it;
        if (current_thread->GetPriority() >= thread->GetPriority()) {
            thread_list.insert(it, thread);
            return;
        }
        ++it;
    }
    thread_list.push_back(std::move(thread));
}

void AddressArbiter::RemoveThread(std::shared_ptr<Thread> thread) {
    const VAddr arb_addr = thread->GetArbiterWaitAddress();
    std::list<std::shared_ptr<Thread>>& thread_list = arb_threads[arb_addr];
    auto it = thread_list.begin();
    while (it != thread_list.end()) {
        const std::shared_ptr<Thread>& current_thread = *it;
        if (current_thread.get() == thread.get()) {
            thread_list.erase(it);
            return;
        }
        ++it;
    }
    UNREACHABLE();
}

std::vector<std::shared_ptr<Thread>> AddressArbiter::GetThreadsWaitingOnAddress(VAddr address) {
    std::vector<std::shared_ptr<Thread>> result;
    std::list<std::shared_ptr<Thread>>& thread_list = arb_threads[address];
    auto it = thread_list.begin();
    while (it != thread_list.end()) {
        std::shared_ptr<Thread> current_thread = *it;
        result.push_back(std::move(current_thread));
        ++it;
    }
    return result;
}
} // namespace Kernel
