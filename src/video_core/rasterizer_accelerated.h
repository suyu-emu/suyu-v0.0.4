// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <mutex>

#include <boost/icl/interval_map.hpp>

#include "common/common_types.h"
#include "video_core/rasterizer_interface.h"

namespace VideoCore {

/// Implements the shared part in GPU accelerated rasterizers in RasterizerInterface.
class RasterizerAccelerated : public RasterizerInterface {
public:
    explicit RasterizerAccelerated();
    ~RasterizerAccelerated() override;

    void UpdatePagesCachedCount(VAddr addr, u64 size, int delta) override;

private:
    using CachedPageMap = boost::icl::interval_map<u64, int>;
    CachedPageMap cached_pages;

    std::mutex pages_mutex;
};

} // namespace VideoCore
