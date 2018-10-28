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

void CallSVC(u32 immediate);

} // namespace Kernel
