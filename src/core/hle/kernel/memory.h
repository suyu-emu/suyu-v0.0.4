// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>

#include "common/common_types.h"

namespace Kernel {

class VMManager;
enum class MemoryRegion : u16;
struct AddressMapping;

struct MemoryRegionInfo {
    u64 base; // Not an address, but offset from start of FCRAM
    u64 size;
    u64 used;

    std::shared_ptr<std::vector<u8>> linear_heap_memory;
};

void MemoryInit(u32 mem_type);
void MemoryShutdown();
MemoryRegionInfo* GetMemoryRegion(MemoryRegion region);

void HandleSpecialMapping(VMManager& address_space, const AddressMapping& mapping);
void MapSharedPages(VMManager& address_space);

extern MemoryRegionInfo memory_regions[3];
} // namespace Kernel
