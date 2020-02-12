// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <list>
#include <memory>
#include <unordered_map>
#include <vector>

#include "common/common_types.h"

union ResultCode;

namespace Core {
class System;
}

namespace Kernel {

class Thread;

class AddressArbiter {
public:
    enum class ArbitrationType {
        WaitIfLessThan = 0,
        DecrementAndWaitIfLessThan = 1,
        WaitIfEqual = 2,
    };

    enum class SignalType {
        Signal = 0,
        IncrementAndSignalIfEqual = 1,
        ModifyByWaitingCountAndSignalIfEqual = 2,
    };

    explicit AddressArbiter(Core::System& system);
    ~AddressArbiter();

    AddressArbiter(const AddressArbiter&) = delete;
    AddressArbiter& operator=(const AddressArbiter&) = delete;

    AddressArbiter(AddressArbiter&&) = default;
    AddressArbiter& operator=(AddressArbiter&&) = delete;

    /// Signals an address being waited on with a particular signaling type.
    ResultCode SignalToAddress(VAddr address, SignalType type, s32 value, s32 num_to_wake);

    /// Waits on an address with a particular arbitration type.
    ResultCode WaitForAddress(VAddr address, ArbitrationType type, s32 value, s64 timeout_ns);

    /// Removes a thread from the container and resets its address arbiter adress to 0
    void HandleWakeupThread(std::shared_ptr<Thread> thread);

private:
    /// Signals an address being waited on.
    ResultCode SignalToAddressOnly(VAddr address, s32 num_to_wake);

    /// Signals an address being waited on and increments its value if equal to the value argument.
    ResultCode IncrementAndSignalToAddressIfEqual(VAddr address, s32 value, s32 num_to_wake);

    /// Signals an address being waited on and modifies its value based on waiting thread count if
    /// equal to the value argument.
    ResultCode ModifyByWaitingCountAndSignalToAddressIfEqual(VAddr address, s32 value,
                                                             s32 num_to_wake);

    /// Waits on an address if the value passed is less than the argument value,
    /// optionally decrementing.
    ResultCode WaitForAddressIfLessThan(VAddr address, s32 value, s64 timeout,
                                        bool should_decrement);

    /// Waits on an address if the value passed is equal to the argument value.
    ResultCode WaitForAddressIfEqual(VAddr address, s32 value, s64 timeout);

    // Waits on the given address with a timeout in nanoseconds
    ResultCode WaitForAddressImpl(VAddr address, s64 timeout);

    /// Wake up num_to_wake (or all) threads in a vector.
    void WakeThreads(const std::vector<std::shared_ptr<Thread>>& waiting_threads, s32 num_to_wake);

    /// Insert a thread into the address arbiter container
    void InsertThread(std::shared_ptr<Thread> thread);

    /// Removes a thread from the address arbiter container
    void RemoveThread(std::shared_ptr<Thread> thread);

    // Gets the threads waiting on an address.
    std::vector<std::shared_ptr<Thread>> GetThreadsWaitingOnAddress(VAddr address) const;

    /// List of threads waiting for a address arbiter
    std::unordered_map<VAddr, std::list<std::shared_ptr<Thread>>> arb_threads;

    Core::System& system;
};

} // namespace Kernel
