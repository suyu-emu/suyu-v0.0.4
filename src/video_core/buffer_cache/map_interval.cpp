// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>

#include "video_core/buffer_cache/map_interval.h"

namespace VideoCommon {

MapIntervalAllocator::MapIntervalAllocator() {
    FillFreeList(first_chunk);
}

MapIntervalAllocator::~MapIntervalAllocator() = default;

void MapIntervalAllocator::AllocateNewChunk() {
    *new_chunk = std::make_unique<Chunk>();
    FillFreeList(**new_chunk);
    new_chunk = &(*new_chunk)->next;
}

void MapIntervalAllocator::FillFreeList(Chunk& chunk) {
    const std::size_t old_size = free_list.size();
    free_list.resize(old_size + chunk.data.size());
    std::transform(chunk.data.rbegin(), chunk.data.rend(), free_list.begin() + old_size,
                   [](MapInterval& interval) { return &interval; });
}

} // namespace VideoCommon
