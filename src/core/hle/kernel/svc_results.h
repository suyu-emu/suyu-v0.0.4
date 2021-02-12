// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/result.h"

namespace Kernel {

// Confirmed Switch kernel error codes

constexpr ResultCode ResultMaxConnectionsReached{ErrorModule::Kernel, 7};
constexpr ResultCode ResultInvalidCapabilityDescriptor{ErrorModule::Kernel, 14};
constexpr ResultCode ResultNoSynchronizationObject{ErrorModule::Kernel, 57};
constexpr ResultCode ResultTerminationRequested{ErrorModule::Kernel, 59};
constexpr ResultCode ResultInvalidSize{ErrorModule::Kernel, 101};
constexpr ResultCode ResultInvalidAddress{ErrorModule::Kernel, 102};
constexpr ResultCode ResultOutOfResource{ErrorModule::Kernel, 103};
constexpr ResultCode ResultOutOfMemory{ErrorModule::Kernel, 104};
constexpr ResultCode ResultHandleTableFull{ErrorModule::Kernel, 105};
constexpr ResultCode ResultInvalidCurrentMemory{ErrorModule::Kernel, 106};
constexpr ResultCode ResultInvalidMemoryPermissions{ErrorModule::Kernel, 108};
constexpr ResultCode ResultInvalidMemoryRange{ErrorModule::Kernel, 110};
constexpr ResultCode ResultInvalidPriority{ErrorModule::Kernel, 112};
constexpr ResultCode ResultInvalidCoreId{ErrorModule::Kernel, 113};
constexpr ResultCode ResultInvalidHandle{ErrorModule::Kernel, 114};
constexpr ResultCode ResultInvalidPointer{ErrorModule::Kernel, 115};
constexpr ResultCode ResultInvalidCombination{ErrorModule::Kernel, 116};
constexpr ResultCode ResultTimedOut{ErrorModule::Kernel, 117};
constexpr ResultCode ResultCancelled{ErrorModule::Kernel, 118};
constexpr ResultCode ResultOutOfRange{ErrorModule::Kernel, 119};
constexpr ResultCode ResultInvalidEnumValue{ErrorModule::Kernel, 120};
constexpr ResultCode ResultNotFound{ErrorModule::Kernel, 121};
constexpr ResultCode ResultBusy{ErrorModule::Kernel, 122};
constexpr ResultCode ResultSessionClosedByRemote{ErrorModule::Kernel, 123};
constexpr ResultCode ResultInvalidState{ErrorModule::Kernel, 125};
constexpr ResultCode ResultReservedValue{ErrorModule::Kernel, 126};
constexpr ResultCode ResultResourceLimitedExceeded{ErrorModule::Kernel, 132};

} // namespace Kernel
