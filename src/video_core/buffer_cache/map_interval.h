// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <boost/functional/hash.hpp>
#include "common/common_types.h"
#include "video_core/gpu.h"

namespace VideoCommon {

struct MapInterval {
    MapInterval(const CacheAddr start, const CacheAddr end) : start{start}, end{end} {}
    CacheAddr start;
    CacheAddr end;
    bool IsInside(const CacheAddr other_start, const CacheAddr other_end) const {
        return (start <= other_start && other_end <= end);
    }

    bool operator==(const MapInterval& rhs) const {
        return std::tie(start, end) == std::tie(rhs.start, rhs.end);
    }

    bool operator!=(const MapInterval& rhs) const {
        return !operator==(rhs);
    }
};

struct MapInfo {
    GPUVAddr gpu_addr;
    VAddr cpu_addr;
};

} // namespace VideoCommon

namespace std {

template <>
struct hash<VideoCommon::MapInterval> {
    std::size_t operator()(const VideoCommon::MapInterval& k) const noexcept {
        std::size_t a = std::hash<CacheAddr>()(k.start);
        boost::hash_combine(a, std::hash<CacheAddr>()(k.end));
        return a;
    }
};

} // namespace std
