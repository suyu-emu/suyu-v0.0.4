// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <mutex>

#include <boost/icl/interval_map.hpp>
#include <boost/range/iterator_range.hpp>

#include "common/assert.h"
#include "common/common_types.h"
#include "core/memory.h"
#include "video_core/rasterizer_accelerated.h"

namespace VideoCore {

namespace {

template <typename Map, typename Interval>
constexpr auto RangeFromInterval(Map& map, const Interval& interval) {
    return boost::make_iterator_range(map.equal_range(interval));
}

} // Anonymous namespace

RasterizerAccelerated::RasterizerAccelerated(Core::Memory::Memory& cpu_memory_)
    : cpu_memory{cpu_memory_} {}

RasterizerAccelerated::~RasterizerAccelerated() = default;

void RasterizerAccelerated::UpdatePagesCachedCount(VAddr addr, u64 size, int delta) {
    std::lock_guard lock{pages_mutex};
    const u64 page_start{addr >> Core::Memory::PAGE_BITS};
    const u64 page_end{(addr + size + Core::Memory::PAGE_SIZE - 1) >> Core::Memory::PAGE_BITS};

    // Interval maps will erase segments if count reaches 0, so if delta is negative we have to
    // subtract after iterating
    const auto pages_interval = CachedPageMap::interval_type::right_open(page_start, page_end);
    if (delta > 0) {
        cached_pages.add({pages_interval, delta});
    }

    for (const auto& pair : RangeFromInterval(cached_pages, pages_interval)) {
        const auto interval = pair.first & pages_interval;
        const int count = pair.second;

        const VAddr interval_start_addr = boost::icl::first(interval) << Core::Memory::PAGE_BITS;
        const VAddr interval_end_addr = boost::icl::last_next(interval) << Core::Memory::PAGE_BITS;
        const u64 interval_size = interval_end_addr - interval_start_addr;

        if (delta > 0 && count == delta) {
            cpu_memory.RasterizerMarkRegionCached(interval_start_addr, interval_size, true);
        } else if (delta < 0 && count == -delta) {
            cpu_memory.RasterizerMarkRegionCached(interval_start_addr, interval_size, false);
        } else {
            ASSERT(count >= 0);
        }
    }

    if (delta < 0) {
        cached_pages.add({pages_interval, delta});
    }
}

} // namespace VideoCore
