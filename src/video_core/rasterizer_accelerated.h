// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <mutex>
#include <boost/icl/interval_map.hpp>

#include "common/common_types.h"
#include "video_core/rasterizer_interface.h"

namespace Core::Memory {
class Memory;
}

namespace VideoCore {

/// Implements the shared part in GPU accelerated rasterizers in RasterizerInterface.
class RasterizerAccelerated : public RasterizerInterface {
public:
    explicit RasterizerAccelerated(Core::Memory::Memory& cpu_memory_);
    ~RasterizerAccelerated() override;

    void UpdatePagesCachedCount(VAddr addr, u64 size, bool cache) override;

private:
    using PageIndex = VAddr;
    using PageReferenceCount = u16;

    using IntervalMap = boost::icl::interval_map<PageIndex, PageReferenceCount>;
    using IntervalType = IntervalMap::interval_type;

    IntervalMap map;
    std::mutex map_lock;
    Core::Memory::Memory& cpu_memory;
};

} // namespace VideoCore
