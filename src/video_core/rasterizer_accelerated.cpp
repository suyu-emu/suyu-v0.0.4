// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/common_types.h"
#include "common/div_ceil.h"
#include "core/memory.h"
#include "video_core/rasterizer_accelerated.h"

namespace VideoCore {

RasterizerAccelerated::RasterizerAccelerated(Core::Memory::Memory& cpu_memory_)
    : cpu_memory{cpu_memory_} {}

RasterizerAccelerated::~RasterizerAccelerated() = default;

void RasterizerAccelerated::UpdatePagesCachedCount(VAddr addr, u64 size, int delta) {
    const auto page_end = Common::DivCeil(addr + size, Core::Memory::PAGE_SIZE);
    for (auto page = addr >> Core::Memory::PAGE_BITS; page != page_end; ++page) {
        auto& count = cached_pages.at(page >> 3).Count(page);

        ASSERT_MSG(count < UINT8_MAX, "Count may exceed UINT8_MAX!");

        count += delta;

        // Assume delta is either -1 or 1
        if (count == 0) {
            cpu_memory.RasterizerMarkRegionCached(page << Core::Memory::PAGE_BITS,
                                                  Core::Memory::PAGE_SIZE, false);
        } else if (count == 1 && delta > 0) {
            cpu_memory.RasterizerMarkRegionCached(page << Core::Memory::PAGE_BITS,
                                                  Core::Memory::PAGE_SIZE, true);
        } else {
            ASSERT(count >= 0);
        }
    }
}

} // namespace VideoCore
