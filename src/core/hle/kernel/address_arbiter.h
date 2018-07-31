// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

union ResultCode;

namespace Kernel {

namespace AddressArbiter {
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

ResultCode SignalToAddress(VAddr address, s32 num_to_wake);
ResultCode IncrementAndSignalToAddressIfEqual(VAddr address, s32 value, s32 num_to_wake);
ResultCode ModifyByWaitingCountAndSignalToAddressIfEqual(VAddr address, s32 value, s32 num_to_wake);

ResultCode WaitForAddressIfLessThan(VAddr address, s32 value, s64 timeout, bool should_decrement);
ResultCode WaitForAddressIfEqual(VAddr address, s32 value, s64 timeout);
} // namespace AddressArbiter

} // namespace Kernel
