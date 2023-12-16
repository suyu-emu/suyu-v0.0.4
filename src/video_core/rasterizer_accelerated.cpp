// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <atomic>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/common_types.h"
#include "common/div_ceil.h"
#include "core/memory.h"
#include "video_core/rasterizer_accelerated.h"

namespace VideoCore {

static constexpr u16 IdentityValue = 1;

using namespace Core::Memory;

RasterizerAccelerated::RasterizerAccelerated(Memory& cpu_memory_) : map{}, cpu_memory{cpu_memory_} {
    // We are tracking CPU memory, which cannot map more than 39 bits.
    const VAddr start_address = 0;
    const VAddr end_address = (1ULL << 39);
    const IntervalType address_space_interval(start_address, end_address);
    const auto value = std::make_pair(address_space_interval, IdentityValue);

    map.add(value);
}

RasterizerAccelerated::~RasterizerAccelerated() = default;

void RasterizerAccelerated::UpdatePagesCachedCount(VAddr addr, u64 size, bool cache) {
    std::scoped_lock lk{map_lock};

    // Align sizes.
    addr = Common::AlignDown(addr, YUZU_PAGESIZE);
    size = Common::AlignUp(size, YUZU_PAGESIZE);

    // Declare the overall interval we are going to operate on.
    const VAddr start_address = addr;
    const VAddr end_address = addr + size;
    const IntervalType modification_range(start_address, end_address);

    // Find the boundaries of where to iterate.
    const auto lower = map.lower_bound(modification_range);
    const auto upper = map.upper_bound(modification_range);

    // Iterate over the contained intervals.
    for (auto it = lower; it != upper; it++) {
        // Intersect interval range with modification range.
        const auto current_range = modification_range & it->first;

        // Calculate the address and size to operate over.
        const auto current_addr = current_range.lower();
        const auto current_size = current_range.upper() - current_addr;

        // Get the current value of the range.
        const auto value = it->second;

        if (cache && value == IdentityValue) {
            // If we are going to cache, and the value is not yet referenced, then cache this range.
            cpu_memory.RasterizerMarkRegionCached(current_addr, current_size, true);
        } else if (!cache && value == IdentityValue + 1) {
            // If we are going to uncache, and this is the last reference, then uncache this range.
            cpu_memory.RasterizerMarkRegionCached(current_addr, current_size, false);
        }
    }

    // Update the set.
    const auto value = std::make_pair(modification_range, IdentityValue);
    if (cache) {
        map.add(value);
    } else {
        map.subtract(value);
    }
}

} // namespace VideoCore
