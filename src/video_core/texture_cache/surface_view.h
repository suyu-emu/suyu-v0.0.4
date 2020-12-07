// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>

#include "common/common_types.h"
#include "video_core/surface.h"
#include "video_core/texture_cache/surface_params.h"

namespace VideoCommon {

struct ViewParams {
    constexpr explicit ViewParams(VideoCore::Surface::SurfaceTarget target_, u32 base_layer_,
                                  u32 num_layers_, u32 base_level_, u32 num_levels_)
        : target{target_}, base_layer{base_layer_}, num_layers{num_layers_},
          base_level{base_level_}, num_levels{num_levels_} {}

    std::size_t Hash() const;

    bool operator==(const ViewParams& rhs) const;
    bool operator!=(const ViewParams& rhs) const;

    bool IsLayered() const {
        switch (target) {
        case VideoCore::Surface::SurfaceTarget::Texture1DArray:
        case VideoCore::Surface::SurfaceTarget::Texture2DArray:
        case VideoCore::Surface::SurfaceTarget::TextureCubemap:
        case VideoCore::Surface::SurfaceTarget::TextureCubeArray:
            return true;
        default:
            return false;
        }
    }

    VideoCore::Surface::SurfaceTarget target{};
    u32 base_layer{};
    u32 num_layers{};
    u32 base_level{};
    u32 num_levels{};
};

class ViewBase {
public:
    constexpr explicit ViewBase(const ViewParams& view_params) : params{view_params} {}

    constexpr const ViewParams& GetViewParams() const {
        return params;
    }

protected:
    ViewParams params;
};

} // namespace VideoCommon

namespace std {

template <>
struct hash<VideoCommon::ViewParams> {
    std::size_t operator()(const VideoCommon::ViewParams& k) const noexcept {
        return k.Hash();
    }
};

} // namespace std
