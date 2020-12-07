// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <unordered_map>

#include "common/common_types.h"
#include "video_core/renderer_vulkan/vk_image.h"
#include "video_core/renderer_vulkan/vk_memory_manager.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/wrapper.h"
#include "video_core/texture_cache/surface_base.h"
#include "video_core/texture_cache/texture_cache.h"

namespace VideoCore {
class RasterizerInterface;
}

namespace Vulkan {

class RasterizerVulkan;
class VKDevice;
class VKScheduler;
class VKStagingBufferPool;

class CachedSurfaceView;
class CachedSurface;

using Surface = std::shared_ptr<CachedSurface>;
using View = std::shared_ptr<CachedSurfaceView>;
using TextureCacheBase = VideoCommon::TextureCache<Surface, View>;

using VideoCommon::SurfaceParams;
using VideoCommon::ViewParams;

class CachedSurface final : public VideoCommon::SurfaceBase<View> {
    friend CachedSurfaceView;

public:
    explicit CachedSurface(const VKDevice& device_, VKMemoryManager& memory_manager_,
                           VKScheduler& scheduler_, VKStagingBufferPool& staging_pool_,
                           GPUVAddr gpu_addr_, const SurfaceParams& params_);
    ~CachedSurface();

    void UploadTexture(const std::vector<u8>& staging_buffer) override;
    void DownloadTexture(std::vector<u8>& staging_buffer) override;

    void FullTransition(VkPipelineStageFlags new_stage_mask, VkAccessFlags new_access,
                        VkImageLayout new_layout) {
        image->Transition(0, static_cast<u32>(params.GetNumLayers()), 0, params.num_levels,
                          new_stage_mask, new_access, new_layout);
    }

    void Transition(u32 base_layer, u32 num_layers, u32 base_level, u32 num_levels,
                    VkPipelineStageFlags new_stage_mask, VkAccessFlags new_access,
                    VkImageLayout new_layout) {
        image->Transition(base_layer, num_layers, base_level, num_levels, new_stage_mask,
                          new_access, new_layout);
    }

    VKImage& GetImage() {
        return *image;
    }

    const VKImage& GetImage() const {
        return *image;
    }

    VkImage GetImageHandle() const {
        return *image->GetHandle();
    }

    VkImageAspectFlags GetAspectMask() const {
        return image->GetAspectMask();
    }

    VkBufferView GetBufferViewHandle() const {
        return *buffer_view;
    }

protected:
    void DecorateSurfaceName() override;

    View CreateView(const ViewParams& view_params) override;

private:
    void UploadBuffer(const std::vector<u8>& staging_buffer);

    void UploadImage(const std::vector<u8>& staging_buffer);

    VkBufferImageCopy GetBufferImageCopy(u32 level) const;

    VkImageSubresourceRange GetImageSubresourceRange() const;

    const VKDevice& device;
    VKMemoryManager& memory_manager;
    VKScheduler& scheduler;
    VKStagingBufferPool& staging_pool;

    std::optional<VKImage> image;
    vk::Buffer buffer;
    vk::BufferView buffer_view;
    VKMemoryCommit commit;

    VkFormat format = VK_FORMAT_UNDEFINED;
};

class CachedSurfaceView final : public VideoCommon::ViewBase {
public:
    explicit CachedSurfaceView(const VKDevice& device_, CachedSurface& surface_,
                               const ViewParams& view_params_);
    ~CachedSurfaceView();

    VkImageView GetImageView(Tegra::Texture::SwizzleSource x_source,
                             Tegra::Texture::SwizzleSource y_source,
                             Tegra::Texture::SwizzleSource z_source,
                             Tegra::Texture::SwizzleSource w_source);

    VkImageView GetAttachment();

    bool IsSameSurface(const CachedSurfaceView& rhs) const {
        return &surface == &rhs.surface;
    }

    u32 GetWidth() const {
        return surface_params.GetMipWidth(base_level);
    }

    u32 GetHeight() const {
        return surface_params.GetMipHeight(base_level);
    }

    u32 GetNumLayers() const {
        return num_layers;
    }

    bool IsBufferView() const {
        return buffer_view;
    }

    VkImage GetImage() const {
        return image;
    }

    VkBufferView GetBufferView() const {
        return buffer_view;
    }

    VkImageSubresourceRange GetImageSubresourceRange() const {
        return {aspect_mask, base_level, num_levels, base_layer, num_layers};
    }

    VkImageSubresourceLayers GetImageSubresourceLayers() const {
        return {surface.GetAspectMask(), base_level, base_layer, num_layers};
    }

    void Transition(VkImageLayout new_layout, VkPipelineStageFlags new_stage_mask,
                    VkAccessFlags new_access) const {
        surface.Transition(base_layer, num_layers, base_level, num_levels, new_stage_mask,
                           new_access, new_layout);
    }

    void MarkAsModified(u64 tick) {
        surface.MarkAsModified(true, tick);
    }

private:
    // Store a copy of these values to avoid double dereference when reading them
    const SurfaceParams surface_params;
    const VkImage image;
    const VkBufferView buffer_view;
    const VkImageAspectFlags aspect_mask;

    const VKDevice& device;
    CachedSurface& surface;
    const u32 base_level;
    const u32 num_levels;
    const VkImageViewType image_view_type;
    u32 base_layer = 0;
    u32 num_layers = 0;
    u32 base_slice = 0;
    u32 num_slices = 0;

    VkImageView last_image_view = nullptr;
    u32 last_swizzle = 0;

    vk::ImageView render_target;
    std::unordered_map<u32, vk::ImageView> view_cache;
};

class VKTextureCache final : public TextureCacheBase {
public:
    explicit VKTextureCache(VideoCore::RasterizerInterface& rasterizer_,
                            Tegra::Engines::Maxwell3D& maxwell3d_,
                            Tegra::MemoryManager& gpu_memory_, const VKDevice& device_,
                            VKMemoryManager& memory_manager_, VKScheduler& scheduler_,
                            VKStagingBufferPool& staging_pool_);
    ~VKTextureCache();

private:
    Surface CreateSurface(GPUVAddr gpu_addr, const SurfaceParams& params) override;

    void ImageCopy(Surface& src_surface, Surface& dst_surface,
                   const VideoCommon::CopyParams& copy_params) override;

    void ImageBlit(View& src_view, View& dst_view,
                   const Tegra::Engines::Fermi2D::Config& copy_config) override;

    void BufferCopy(Surface& src_surface, Surface& dst_surface) override;

    const VKDevice& device;
    VKMemoryManager& memory_manager;
    VKScheduler& scheduler;
    VKStagingBufferPool& staging_pool;
};

} // namespace Vulkan
