// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>

#include "common/common_types.h"

namespace VideoCommon {

struct ViewKey {
    std::size_t Hash() const;

    bool operator==(const ViewKey& rhs) const;

    u32 base_layer{};
    u32 num_layers{};
    u32 base_level{};
    u32 num_levels{};
};

} // namespace VideoCommon

namespace std {

template <>
struct hash<VideoCommon::ViewKey> {
    std::size_t operator()(const VideoCommon::ViewKey& k) const noexcept {
        return k.Hash();
    }
};

} // namespace std
