// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <tuple>
#include <unordered_map>

#include <boost/container/static_vector.hpp>
#include <boost/functional/hash.hpp>

#include "video_core/engines/maxwell_3d.h"
#include "video_core/renderer_vulkan/wrapper.h"
#include "video_core/surface.h"

namespace Vulkan {

class VKDevice;

// TODO(Rodrigo): Optimize this structure for faster hashing

struct RenderPassParams {
    struct ColorAttachment {
        u32 index = 0;
        VideoCore::Surface::PixelFormat pixel_format = VideoCore::Surface::PixelFormat::Invalid;
        bool is_texception = false;

        std::size_t Hash() const noexcept {
            return static_cast<std::size_t>(pixel_format) |
                   static_cast<std::size_t>(is_texception) << 6 |
                   static_cast<std::size_t>(index) << 7;
        }

        bool operator==(const ColorAttachment& rhs) const noexcept {
            return std::tie(index, pixel_format, is_texception) ==
                   std::tie(rhs.index, rhs.pixel_format, rhs.is_texception);
        }
    };

    boost::container::static_vector<ColorAttachment,
                                    Tegra::Engines::Maxwell3D::Regs::NumRenderTargets>
        color_attachments{};
    // TODO(Rodrigo): Unify has_zeta into zeta_pixel_format and zeta_component_type.
    VideoCore::Surface::PixelFormat zeta_pixel_format = VideoCore::Surface::PixelFormat::Invalid;
    bool has_zeta = false;
    bool zeta_texception = false;

    std::size_t Hash() const noexcept {
        std::size_t hash = 0;
        for (const auto& rt : color_attachments) {
            boost::hash_combine(hash, rt.Hash());
        }
        boost::hash_combine(hash, zeta_pixel_format);
        boost::hash_combine(hash, has_zeta);
        boost::hash_combine(hash, zeta_texception);
        return hash;
    }

    bool operator==(const RenderPassParams& rhs) const {
        return std::tie(color_attachments, zeta_pixel_format, has_zeta, zeta_texception) ==
               std::tie(rhs.color_attachments, rhs.zeta_pixel_format, rhs.has_zeta,
                        rhs.zeta_texception);
    }
};

} // namespace Vulkan

namespace std {

template <>
struct hash<Vulkan::RenderPassParams> {
    std::size_t operator()(const Vulkan::RenderPassParams& k) const noexcept {
        return k.Hash();
    }
};

} // namespace std

namespace Vulkan {

class VKRenderPassCache final {
public:
    explicit VKRenderPassCache(const VKDevice& device);
    ~VKRenderPassCache();

    VkRenderPass GetRenderPass(const RenderPassParams& params);

private:
    vk::RenderPass CreateRenderPass(const RenderPassParams& params) const;

    const VKDevice& device;
    std::unordered_map<RenderPassParams, vk::RenderPass> cache;
};

} // namespace Vulkan
