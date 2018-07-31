// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <memory>
#include <utility>
#include <vector>
#include "common/assert.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/hle/kernel/memory.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/vm_manager.h"
#include "core/memory.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Kernel {

MemoryRegionInfo memory_regions[3];

/// Size of the APPLICATION, SYSTEM and BASE memory regions (respectively) for each system
/// memory configuration type.
static const u32 memory_region_sizes[8][3] = {
    // Old 3DS layouts
    {0x04000000, 0x02C00000, 0x01400000}, // 0
    {/* This appears to be unused. */},   // 1
    {0x06000000, 0x00C00000, 0x01400000}, // 2
    {0x05000000, 0x01C00000, 0x01400000}, // 3
    {0x04800000, 0x02400000, 0x01400000}, // 4
    {0x02000000, 0x04C00000, 0x01400000}, // 5

    // New 3DS layouts
    {0x07C00000, 0x06400000, 0x02000000}, // 6
    {0x0B200000, 0x02E00000, 0x02000000}, // 7
};

void MemoryInit(u32 mem_type) {
    // TODO(yuriks): On the n3DS, all o3DS configurations (<=5) are forced to 6 instead.
    ASSERT_MSG(mem_type <= 5, "New 3DS memory configuration aren't supported yet!");
    ASSERT(mem_type != 1);

    // The kernel allocation regions (APPLICATION, SYSTEM and BASE) are laid out in sequence, with
    // the sizes specified in the memory_region_sizes table.
    VAddr base = 0;
    for (int i = 0; i < 3; ++i) {
        memory_regions[i].base = base;
        memory_regions[i].size = memory_region_sizes[mem_type][i];
        memory_regions[i].used = 0;
        memory_regions[i].linear_heap_memory = std::make_shared<std::vector<u8>>();
        // Reserve enough space for this region of FCRAM.
        // We do not want this block of memory to be relocated when allocating from it.
        memory_regions[i].linear_heap_memory->reserve(memory_regions[i].size);

        base += memory_regions[i].size;
    }

    // We must've allocated the entire FCRAM by the end
    ASSERT(base == Memory::FCRAM_SIZE);
}

void MemoryShutdown() {
    for (auto& region : memory_regions) {
        region.base = 0;
        region.size = 0;
        region.used = 0;
        region.linear_heap_memory = nullptr;
    }
}

MemoryRegionInfo* GetMemoryRegion(MemoryRegion region) {
    switch (region) {
    case MemoryRegion::APPLICATION:
        return &memory_regions[0];
    case MemoryRegion::SYSTEM:
        return &memory_regions[1];
    case MemoryRegion::BASE:
        return &memory_regions[2];
    default:
        UNREACHABLE();
    }
}

void HandleSpecialMapping(VMManager& address_space, const AddressMapping& mapping) {}

void MapSharedPages(VMManager& address_space) {}

} // namespace Kernel
