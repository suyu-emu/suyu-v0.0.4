// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <tuple>

#include "common/common_types.h"
#include "video_core/texture_cache/surface_view.h"

namespace VideoCommon {

std::size_t ViewKey::Hash() const {
    return static_cast<std::size_t>(base_layer) ^ static_cast<std::size_t>(num_layers << 16) ^
           (static_cast<std::size_t>(base_level) << 32) ^
           (static_cast<std::size_t>(num_levels) << 48);
}

bool ViewKey::operator==(const ViewKey& rhs) const {
    return std::tie(base_layer, num_layers, base_level, num_levels) ==
           std::tie(rhs.base_layer, rhs.num_layers, rhs.base_level, rhs.num_levels);
}

} // namespace VideoCommon
