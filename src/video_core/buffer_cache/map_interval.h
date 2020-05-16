// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "video_core/gpu.h"

namespace VideoCommon {

struct MapInterval {
    constexpr explicit MapInterval() noexcept = default;

    constexpr explicit MapInterval(VAddr start, VAddr end, GPUVAddr gpu_addr) noexcept
        : start{start}, end{end}, gpu_addr{gpu_addr} {}

    constexpr bool IsInside(VAddr other_start, VAddr other_end) const noexcept {
        return (start <= other_start && other_end <= end);
    }

    constexpr bool operator==(const MapInterval& rhs) const noexcept {
        return start == rhs.start && end == rhs.end;
    }

    constexpr bool operator!=(const MapInterval& rhs) const noexcept {
        return !operator==(rhs);
    }

    constexpr void MarkAsModified(bool is_modified_, u64 ticks_) noexcept {
        is_modified = is_modified_;
        ticks = ticks_;
    }

    VAddr start = 0;
    VAddr end = 0;
    GPUVAddr gpu_addr = 0;
    VAddr cpu_addr = 0;
    u64 ticks = 0;
    bool is_written = false;
    bool is_modified = false;
    bool is_registered = false;
    bool is_memory_marked = false;
    bool is_sync_pending = false;
};

} // namespace VideoCommon
