// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Kernel {

struct MemoryInfo {
    u64 base_address;
    u64 size;
    u32 type;
    u32 attributes;
    u32 permission;
    u32 device_refcount;
    u32 ipc_refcount;
    INSERT_PADDING_WORDS(1);
};
static_assert(sizeof(MemoryInfo) == 0x28, "MemoryInfo has incorrect size.");

struct PageInfo {
    u64 flags;
};

/// Values accepted by svcGetInfo
enum class GetInfoType : u64 {
    // 1.0.0+
    AllowedCpuIdBitmask = 0,
    AllowedThreadPrioBitmask = 1,
    MapRegionBaseAddr = 2,
    MapRegionSize = 3,
    HeapRegionBaseAddr = 4,
    HeapRegionSize = 5,
    TotalMemoryUsage = 6,
    TotalHeapUsage = 7,
    IsCurrentProcessBeingDebugged = 8,
    ResourceHandleLimit = 9,
    IdleTickCount = 10,
    RandomEntropy = 11,
    PerformanceCounter = 0xF0000002,
    // 2.0.0+
    ASLRRegionBaseAddr = 12,
    ASLRRegionSize = 13,
    NewMapRegionBaseAddr = 14,
    NewMapRegionSize = 15,
    // 3.0.0+
    IsVirtualAddressMemoryEnabled = 16,
    PersonalMmHeapUsage = 17,
    TitleId = 18,
    // 4.0.0+
    PrivilegedProcessId = 19,
    // 5.0.0+
    UserExceptionContextAddr = 20,
};

void CallSVC(u32 immediate);

} // namespace Kernel
