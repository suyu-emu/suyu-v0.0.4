// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <vector>

#include <boost/intrusive/set_hook.hpp>

#include "common/common_types.h"
#include "video_core/gpu.h"

namespace VideoCommon {

struct MapInterval : public boost::intrusive::set_base_hook<boost::intrusive::optimize_size<true>> {
    MapInterval() = default;

    /*implicit*/ MapInterval(VAddr start_) noexcept : start{start_} {}

    explicit MapInterval(VAddr start_, VAddr end_, GPUVAddr gpu_addr_) noexcept
        : start{start_}, end{end_}, gpu_addr{gpu_addr_} {}

    bool IsInside(VAddr other_start, VAddr other_end) const noexcept {
        return start <= other_start && other_end <= end;
    }

    bool Overlaps(VAddr other_start, VAddr other_end) const noexcept {
        return start < other_end && other_start < end;
    }

    void MarkAsModified(bool is_modified_, u64 ticks_) noexcept {
        is_modified = is_modified_;
        ticks = ticks_;
    }

    boost::intrusive::set_member_hook<> member_hook_;
    VAddr start = 0;
    VAddr end = 0;
    GPUVAddr gpu_addr = 0;
    u64 ticks = 0;
    bool is_written = false;
    bool is_modified = false;
    bool is_registered = false;
    bool is_memory_marked = false;
    bool is_sync_pending = false;
};

struct MapIntervalCompare {
    constexpr bool operator()(const MapInterval& lhs, const MapInterval& rhs) const noexcept {
        return lhs.start < rhs.start;
    }
};

class MapIntervalAllocator {
public:
    MapIntervalAllocator();
    ~MapIntervalAllocator();

    MapInterval* Allocate() {
        if (free_list.empty()) {
            AllocateNewChunk();
        }
        MapInterval* const interval = free_list.back();
        free_list.pop_back();
        return interval;
    }

    void Release(MapInterval* interval) {
        free_list.push_back(interval);
    }

private:
    struct Chunk {
        std::unique_ptr<Chunk> next;
        std::array<MapInterval, 0x8000> data;
    };

    void AllocateNewChunk();

    void FillFreeList(Chunk& chunk);

    std::vector<MapInterval*> free_list;

    Chunk first_chunk;

    std::unique_ptr<Chunk>* new_chunk = &first_chunk.next;
};

} // namespace VideoCommon
