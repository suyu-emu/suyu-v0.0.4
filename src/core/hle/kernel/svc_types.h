// Copyright 2020 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_funcs.h"
#include "common/common_types.h"

namespace Kernel::Svc {

enum class MemoryState : u32 {
    Free = 0x00,
    Io = 0x01,
    Static = 0x02,
    Code = 0x03,
    CodeData = 0x04,
    Normal = 0x05,
    Shared = 0x06,
    Alias = 0x07,
    AliasCode = 0x08,
    AliasCodeData = 0x09,
    Ipc = 0x0A,
    Stack = 0x0B,
    ThreadLocal = 0x0C,
    Transferred = 0x0D,
    SharedTransferred = 0x0E,
    SharedCode = 0x0F,
    Inaccessible = 0x10,
    NonSecureIpc = 0x11,
    NonDeviceIpc = 0x12,
    Kernel = 0x13,
    GeneratedCode = 0x14,
    CodeOut = 0x15,
};
DECLARE_ENUM_FLAG_OPERATORS(MemoryState);

enum class MemoryAttribute : u32 {
    Locked = (1 << 0),
    IpcLocked = (1 << 1),
    DeviceShared = (1 << 2),
    Uncached = (1 << 3),
};
DECLARE_ENUM_FLAG_OPERATORS(MemoryAttribute);

enum class MemoryPermission : u32 {
    None = (0 << 0),
    Read = (1 << 0),
    Write = (1 << 1),
    Execute = (1 << 2),
    ReadWrite = Read | Write,
    ReadExecute = Read | Execute,
    DontCare = (1 << 28),
};
DECLARE_ENUM_FLAG_OPERATORS(MemoryPermission);

struct MemoryInfo {
    u64 addr{};
    u64 size{};
    MemoryState state{};
    MemoryAttribute attr{};
    MemoryPermission perm{};
    u32 ipc_refcount{};
    u32 device_refcount{};
    u32 padding{};
};

} // namespace Kernel::Svc
