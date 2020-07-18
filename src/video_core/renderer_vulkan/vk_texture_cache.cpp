// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <memory>
#include <variant>
#include <vector>

#include "common/assert.h"
#include "common/common_types.h"
#include "core/core.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/morton.h"
#include "video_core/renderer_vulkan/maxwell_to_vk.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_memory_manager.h"
#include "video_core/renderer_vulkan/vk_rasterizer.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_staging_buffer_pool.h"
#include "video_core/renderer_vulkan/vk_texture_cache.h"
#include "video_core/renderer_vulkan/wrapper.h"
#include "video_core/surface.h"

namespace Vulkan {

using VideoCore::MortonSwizzle;
using VideoCore::MortonSwizzleMode;

using Tegra::Texture::SwizzleSource;
using VideoCore::Surface::PixelFormat;
using VideoCore::Surface::SurfaceTarget;

namespace {

VkImageType SurfaceTargetToImage(SurfaceTarget target) {
    switch (target) {
    case SurfaceTarget::Texture1D:
    case SurfaceTarget::Texture1DArray:
        return VK_IMAGE_TYPE_1D;
    case SurfaceTarget::Texture2D:
    case SurfaceTarget::Texture2DArray:
    case SurfaceTarget::TextureCubemap:
    case SurfaceTarget::TextureCubeArray:
        return VK_IMAGE_TYPE_2D;
    case SurfaceTarget::Texture3D:
        return VK_IMAGE_TYPE_3D;
    case SurfaceTarget::TextureBuffer:
        UNREACHABLE();
        return {};
    }
    UNREACHABLE_MSG("Unknown texture target={}", static_cast<u32>(target));
    return {};
}

VkImageAspectFlags PixelFormatToImageAspect(PixelFormat pixel_format) {
    if (pixel_format < PixelFormat::MaxColorFormat) {
        return VK_IMAGE_ASPECT_COLOR_BIT;
    } else if (pixel_format < PixelFormat::MaxDepthFormat) {
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    } else if (pixel_format < PixelFormat::MaxDepthStencilFormat) {
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    } else {
        UNREACHABLE_MSG("Invalid pixel format={}", static_cast<int>(pixel_format));
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

VkImageViewType GetImageViewType(SurfaceTarget target) {
    switch (target) {
    case SurfaceTarget::Texture1D:
        return VK_IMAGE_VIEW_TYPE_1D;
    case SurfaceTarget::Texture2D:
        return VK_IMAGE_VIEW_TYPE_2D;
    case SurfaceTarget::Texture3D:
        return VK_IMAGE_VIEW_TYPE_3D;
    case SurfaceTarget::Texture1DArray:
        return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
    case SurfaceTarget::Texture2DArray:
        return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    case SurfaceTarget::TextureCubemap:
        return VK_IMAGE_VIEW_TYPE_CUBE;
    case SurfaceTarget::TextureCubeArray:
        return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
    case SurfaceTarget::TextureBuffer:
        break;
    }
    UNREACHABLE();
    return {};
}

vk::Buffer CreateBuffer(const VKDevice& device, const SurfaceParams& params,
                        std::size_t host_memory_size) {
    // TODO(Rodrigo): Move texture buffer creation to the buffer cache
    return device.GetLogical().CreateBuffer({
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = static_cast<VkDeviceSize>(host_memory_size),
        .usage = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
                 VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    });
}

VkBufferViewCreateInfo GenerateBufferViewCreateInfo(const VKDevice& device,
                                                    const SurfaceParams& params, VkBuffer buffer,
                                                    std::size_t host_memory_size) {
    ASSERT(params.IsBuffer());

    return {
        .sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .buffer = buffer,
        .format =
            MaxwellToVK::SurfaceFormat(device, FormatType::Buffer, params.pixel_format).format,
        .offset = 0,
        .range = static_cast<VkDeviceSize>(host_memory_size),
    };
}

VkImageCreateInfo GenerateImageCreateInfo(const VKDevice& device, const SurfaceParams& params) {
    ASSERT(!params.IsBuffer());

    const auto [format, attachable, storage] =
        MaxwellToVK::SurfaceFormat(device, FormatType::Optimal, params.pixel_format);

    VkImageCreateInfo ci{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .imageType = SurfaceTargetToImage(params.target),
        .format = format,
        .mipLevels = params.num_levels,
        .arrayLayers = static_cast<u32>(params.GetNumLayers()),
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                 VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    if (attachable) {
        ci.usage |= params.IsPixelFormatZeta() ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                                               : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }
    if (storage) {
        ci.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }

    switch (params.target) {
    case SurfaceTarget::TextureCubemap:
    case SurfaceTarget::TextureCubeArray:
        ci.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        [[fallthrough]];
    case SurfaceTarget::Texture1D:
    case SurfaceTarget::Texture1DArray:
    case SurfaceTarget::Texture2D:
    case SurfaceTarget::Texture2DArray:
        ci.extent = {params.width, params.height, 1};
        break;
    case SurfaceTarget::Texture3D:
        ci.flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
        ci.extent = {params.width, params.height, params.depth};
        break;
    case SurfaceTarget::TextureBuffer:
        UNREACHABLE();
    }

    return ci;
}

u32 EncodeSwizzle(Tegra::Texture::SwizzleSource x_source, Tegra::Texture::SwizzleSource y_source,
                  Tegra::Texture::SwizzleSource z_source, Tegra::Texture::SwizzleSource w_source) {
    return (static_cast<u32>(x_source) << 24) | (static_cast<u32>(y_source) << 16) |
           (static_cast<u32>(z_source) << 8) | static_cast<u32>(w_source);
}

} // Anonymous namespace

CachedSurface::CachedSurface(Core::System& system, const VKDevice& device,
                             VKResourceManager& resource_manager, VKMemoryManager& memory_manager,
                             VKScheduler& scheduler, VKStagingBufferPool& staging_pool,
                             GPUVAddr gpu_addr, const SurfaceParams& params)
    : SurfaceBase<View>{gpu_addr, params, device.IsOptimalAstcSupported()}, system{system},
      device{device}, resource_manager{resource_manager},
      memory_manager{memory_manager}, scheduler{scheduler}, staging_pool{staging_pool} {
    if (params.IsBuffer()) {
        buffer = CreateBuffer(device, params, host_memory_size);
        commit = memory_manager.Commit(buffer, false);

        const auto buffer_view_ci =
            GenerateBufferViewCreateInfo(device, params, *buffer, host_memory_size);
        format = buffer_view_ci.format;

        buffer_view = device.GetLogical().CreateBufferView(buffer_view_ci);
    } else {
        const auto image_ci = GenerateImageCreateInfo(device, params);
        format = image_ci.format;

        image.emplace(device, scheduler, image_ci, PixelFormatToImageAspect(params.pixel_format));
        commit = memory_manager.Commit(image->GetHandle(), false);
    }

    // TODO(Rodrigo): Move this to a virtual function.
    u32 num_layers = 1;
    if (params.is_layered || params.target == SurfaceTarget::Texture3D) {
        num_layers = params.depth;
    }
    main_view = CreateView(ViewParams(params.target, 0, num_layers, 0, params.num_levels));
}

CachedSurface::~CachedSurface() = default;

void CachedSurface::UploadTexture(const std::vector<u8>& staging_buffer) {
    // To upload data we have to be outside of a renderpass
    scheduler.RequestOutsideRenderPassOperationContext();

    if (params.IsBuffer()) {
        UploadBuffer(staging_buffer);
    } else {
        UploadImage(staging_buffer);
    }
}

void CachedSurface::DownloadTexture(std::vector<u8>& staging_buffer) {
    UNIMPLEMENTED_IF(params.IsBuffer());

    if (params.pixel_format == VideoCore::Surface::PixelFormat::A1B5G5R5U) {
        LOG_WARNING(Render_Vulkan, "A1B5G5R5 flushing is stubbed");
    }

    // We can't copy images to buffers inside a renderpass
    scheduler.RequestOutsideRenderPassOperationContext();

    FullTransition(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    const auto& buffer = staging_pool.GetUnusedBuffer(host_memory_size, true);
    // TODO(Rodrigo): Do this in a single copy
    for (u32 level = 0; level < params.num_levels; ++level) {
        scheduler.Record([image = *image->GetHandle(), buffer = *buffer.handle,
                          copy = GetBufferImageCopy(level)](vk::CommandBuffer cmdbuf) {
            cmdbuf.CopyImageToBuffer(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, copy);
        });
    }
    scheduler.Finish();

    // TODO(Rodrigo): Use an intern buffer for staging buffers and avoid this unnecessary memcpy.
    std::memcpy(staging_buffer.data(), buffer.commit->Map(host_memory_size), host_memory_size);
}

void CachedSurface::DecorateSurfaceName() {
    // TODO(Rodrigo): Add name decorations
}

View CachedSurface::CreateView(const ViewParams& params) {
    // TODO(Rodrigo): Add name decorations
    return views[params] = std::make_shared<CachedSurfaceView>(device, *this, params);
}

void CachedSurface::UploadBuffer(const std::vector<u8>& staging_buffer) {
    const auto& src_buffer = staging_pool.GetUnusedBuffer(host_memory_size, true);
    std::memcpy(src_buffer.commit->Map(host_memory_size), staging_buffer.data(), host_memory_size);

    scheduler.Record([src_buffer = *src_buffer.handle, dst_buffer = *buffer,
                      size = host_memory_size](vk::CommandBuffer cmdbuf) {
        VkBufferCopy copy;
        copy.srcOffset = 0;
        copy.dstOffset = 0;
        copy.size = size;
        cmdbuf.CopyBuffer(src_buffer, dst_buffer, copy);

        VkBufferMemoryBarrier barrier;
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.pNext = nullptr;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // They'll be ignored anyway
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer = dst_buffer;
        barrier.offset = 0;
        barrier.size = size;
        cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                               0, {}, barrier, {});
    });
}

void CachedSurface::UploadImage(const std::vector<u8>& staging_buffer) {
    const auto& src_buffer = staging_pool.GetUnusedBuffer(host_memory_size, true);
    std::memcpy(src_buffer.commit->Map(host_memory_size), staging_buffer.data(), host_memory_size);

    FullTransition(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    for (u32 level = 0; level < params.num_levels; ++level) {
        const VkBufferImageCopy copy = GetBufferImageCopy(level);
        if (image->GetAspectMask() == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
            scheduler.Record([buffer = *src_buffer.handle, image = *image->GetHandle(),
                              copy](vk::CommandBuffer cmdbuf) {
                std::array<VkBufferImageCopy, 2> copies = {copy, copy};
                copies[0].imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                copies[1].imageSubresource.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
                cmdbuf.CopyBufferToImage(buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                         copies);
            });
        } else {
            scheduler.Record([buffer = *src_buffer.handle, image = *image->GetHandle(),
                              copy](vk::CommandBuffer cmdbuf) {
                cmdbuf.CopyBufferToImage(buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copy);
            });
        }
    }
}

VkBufferImageCopy CachedSurface::GetBufferImageCopy(u32 level) const {
    return {
        .bufferOffset = params.GetHostMipmapLevelOffset(level, is_converted),
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource =
            {
                .aspectMask = image->GetAspectMask(),
                .mipLevel = level,
                .baseArrayLayer = 0,
                .layerCount = static_cast<u32>(params.GetNumLayers()),
            },
        .imageOffset = {.x = 0, .y = 0, .z = 0},
        .imageExtent =
            {
                .width = params.GetMipWidth(level),
                .height = params.GetMipHeight(level),
                .depth = params.target == SurfaceTarget::Texture3D ? params.GetMipDepth(level) : 1U,
            },
    };
}

VkImageSubresourceRange CachedSurface::GetImageSubresourceRange() const {
    return {image->GetAspectMask(), 0, params.num_levels, 0,
            static_cast<u32>(params.GetNumLayers())};
}

CachedSurfaceView::CachedSurfaceView(const VKDevice& device, CachedSurface& surface,
                                     const ViewParams& params)
    : VideoCommon::ViewBase{params}, params{surface.GetSurfaceParams()},
      image{surface.GetImageHandle()}, buffer_view{surface.GetBufferViewHandle()},
      aspect_mask{surface.GetAspectMask()}, device{device}, surface{surface},
      base_level{params.base_level}, num_levels{params.num_levels},
      image_view_type{image ? GetImageViewType(params.target) : VK_IMAGE_VIEW_TYPE_1D} {
    if (image_view_type == VK_IMAGE_VIEW_TYPE_3D) {
        base_layer = 0;
        num_layers = 1;
        base_slice = params.base_layer;
        num_slices = params.num_layers;
    } else {
        base_layer = params.base_layer;
        num_layers = params.num_layers;
    }
}

CachedSurfaceView::~CachedSurfaceView() = default;

VkImageView CachedSurfaceView::GetImageView(SwizzleSource x_source, SwizzleSource y_source,
                                            SwizzleSource z_source, SwizzleSource w_source) {
    const u32 new_swizzle = EncodeSwizzle(x_source, y_source, z_source, w_source);
    if (last_image_view && last_swizzle == new_swizzle) {
        return last_image_view;
    }
    last_swizzle = new_swizzle;

    const auto [entry, is_cache_miss] = view_cache.try_emplace(new_swizzle);
    auto& image_view = entry->second;
    if (!is_cache_miss) {
        return last_image_view = *image_view;
    }

    std::array swizzle{MaxwellToVK::SwizzleSource(x_source), MaxwellToVK::SwizzleSource(y_source),
                       MaxwellToVK::SwizzleSource(z_source), MaxwellToVK::SwizzleSource(w_source)};
    if (params.pixel_format == VideoCore::Surface::PixelFormat::A1B5G5R5U) {
        // A1B5G5R5 is implemented as A1R5G5B5, we have to change the swizzle here.
        std::swap(swizzle[0], swizzle[2]);
    }

    // Games can sample depth or stencil values on textures. This is decided by the swizzle value on
    // hardware. To emulate this on Vulkan we specify it in the aspect.
    VkImageAspectFlags aspect = aspect_mask;
    if (aspect == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
        UNIMPLEMENTED_IF(x_source != SwizzleSource::R && x_source != SwizzleSource::G);
        const bool is_first = x_source == SwizzleSource::R;
        switch (params.pixel_format) {
        case VideoCore::Surface::PixelFormat::Z24S8:
        case VideoCore::Surface::PixelFormat::Z32FS8:
            aspect = is_first ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_STENCIL_BIT;
            break;
        case VideoCore::Surface::PixelFormat::S8Z24:
            aspect = is_first ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT;
            break;
        default:
            aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
            UNIMPLEMENTED();
        }

        // Make sure we sample the first component
        std::transform(
            swizzle.begin(), swizzle.end(), swizzle.begin(), [](VkComponentSwizzle component) {
                return component == VK_COMPONENT_SWIZZLE_G ? VK_COMPONENT_SWIZZLE_R : component;
            });
    }

    if (image_view_type == VK_IMAGE_VIEW_TYPE_3D) {
        ASSERT(base_slice == 0);
        ASSERT(num_slices == params.depth);
    }

    image_view = device.GetLogical().CreateImageView({
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = surface.GetImageHandle(),
        .viewType = image_view_type,
        .format = surface.GetImage().GetFormat(),
        .components =
            {
                .r = swizzle[0],
                .g = swizzle[1],
                .b = swizzle[2],
                .a = swizzle[3],
            },
        .subresourceRange =
            {
                .aspectMask = aspect,
                .baseMipLevel = base_level,
                .levelCount = num_levels,
                .baseArrayLayer = base_layer,
                .layerCount = num_layers,
            },
    });

    return last_image_view = *image_view;
}

VkImageView CachedSurfaceView::GetAttachment() {
    if (render_target) {
        return *render_target;
    }

    VkImageViewCreateInfo ci{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = surface.GetImageHandle(),
        .format = surface.GetImage().GetFormat(),
        .components =
            {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
        .subresourceRange =
            {
                .aspectMask = aspect_mask,
                .baseMipLevel = base_level,
                .levelCount = num_levels,
            },
    };
    if (image_view_type == VK_IMAGE_VIEW_TYPE_3D) {
        ci.viewType = num_slices > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        ci.subresourceRange.baseArrayLayer = base_slice;
        ci.subresourceRange.layerCount = num_slices;
    } else {
        ci.viewType = image_view_type;
        ci.subresourceRange.baseArrayLayer = base_layer;
        ci.subresourceRange.layerCount = num_layers;
    }
    render_target = device.GetLogical().CreateImageView(ci);
    return *render_target;
}

VKTextureCache::VKTextureCache(Core::System& system, VideoCore::RasterizerInterface& rasterizer,
                               const VKDevice& device, VKResourceManager& resource_manager,
                               VKMemoryManager& memory_manager, VKScheduler& scheduler,
                               VKStagingBufferPool& staging_pool)
    : TextureCache(system, rasterizer, device.IsOptimalAstcSupported()), device{device},
      resource_manager{resource_manager}, memory_manager{memory_manager}, scheduler{scheduler},
      staging_pool{staging_pool} {}

VKTextureCache::~VKTextureCache() = default;

Surface VKTextureCache::CreateSurface(GPUVAddr gpu_addr, const SurfaceParams& params) {
    return std::make_shared<CachedSurface>(system, device, resource_manager, memory_manager,
                                           scheduler, staging_pool, gpu_addr, params);
}

void VKTextureCache::ImageCopy(Surface& src_surface, Surface& dst_surface,
                               const VideoCommon::CopyParams& copy_params) {
    const bool src_3d = src_surface->GetSurfaceParams().target == SurfaceTarget::Texture3D;
    const bool dst_3d = dst_surface->GetSurfaceParams().target == SurfaceTarget::Texture3D;
    UNIMPLEMENTED_IF(src_3d);

    // The texture cache handles depth in OpenGL terms, we have to handle it as subresource and
    // dimension respectively.
    const u32 dst_base_layer = dst_3d ? 0 : copy_params.dest_z;
    const u32 dst_offset_z = dst_3d ? copy_params.dest_z : 0;

    const u32 extent_z = dst_3d ? copy_params.depth : 1;
    const u32 num_layers = dst_3d ? 1 : copy_params.depth;

    // We can't copy inside a renderpass
    scheduler.RequestOutsideRenderPassOperationContext();

    src_surface->Transition(copy_params.source_z, copy_params.depth, copy_params.source_level, 1,
                            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    dst_surface->Transition(dst_base_layer, num_layers, copy_params.dest_level, 1,
                            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    const VkImageCopy copy{
        .srcSubresource =
            {
                .aspectMask = src_surface->GetAspectMask(),
                .mipLevel = copy_params.source_level,
                .baseArrayLayer = copy_params.source_z,
                .layerCount = num_layers,
            },
        .srcOffset =
            {
                .x = static_cast<s32>(copy_params.source_x),
                .y = static_cast<s32>(copy_params.source_y),
                .z = 0,
            },
        .dstSubresource =
            {
                .aspectMask = dst_surface->GetAspectMask(),
                .mipLevel = copy_params.dest_level,
                .baseArrayLayer = dst_base_layer,
                .layerCount = num_layers,
            },
        .dstOffset =
            {
                .x = static_cast<s32>(copy_params.dest_x),
                .y = static_cast<s32>(copy_params.dest_y),
                .z = static_cast<s32>(dst_offset_z),
            },
        .extent =
            {
                .width = copy_params.width,
                .height = copy_params.height,
                .depth = extent_z,
            },
    };

    const VkImage src_image = src_surface->GetImageHandle();
    const VkImage dst_image = dst_surface->GetImageHandle();
    scheduler.Record([src_image, dst_image, copy](vk::CommandBuffer cmdbuf) {
        cmdbuf.CopyImage(src_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst_image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copy);
    });
}

void VKTextureCache::ImageBlit(View& src_view, View& dst_view,
                               const Tegra::Engines::Fermi2D::Config& copy_config) {
    // We can't blit inside a renderpass
    scheduler.RequestOutsideRenderPassOperationContext();

    src_view->Transition(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_ACCESS_TRANSFER_READ_BIT);
    dst_view->Transition(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_ACCESS_TRANSFER_WRITE_BIT);

    VkImageBlit blit;
    blit.srcSubresource = src_view->GetImageSubresourceLayers();
    blit.srcOffsets[0].x = copy_config.src_rect.left;
    blit.srcOffsets[0].y = copy_config.src_rect.top;
    blit.srcOffsets[0].z = 0;
    blit.srcOffsets[1].x = copy_config.src_rect.right;
    blit.srcOffsets[1].y = copy_config.src_rect.bottom;
    blit.srcOffsets[1].z = 1;
    blit.dstSubresource = dst_view->GetImageSubresourceLayers();
    blit.dstOffsets[0].x = copy_config.dst_rect.left;
    blit.dstOffsets[0].y = copy_config.dst_rect.top;
    blit.dstOffsets[0].z = 0;
    blit.dstOffsets[1].x = copy_config.dst_rect.right;
    blit.dstOffsets[1].y = copy_config.dst_rect.bottom;
    blit.dstOffsets[1].z = 1;

    const bool is_linear = copy_config.filter == Tegra::Engines::Fermi2D::Filter::Linear;

    scheduler.Record([src_image = src_view->GetImage(), dst_image = dst_view->GetImage(), blit,
                      is_linear](vk::CommandBuffer cmdbuf) {
        cmdbuf.BlitImage(src_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst_image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, blit,
                         is_linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST);
    });
}

void VKTextureCache::BufferCopy(Surface& src_surface, Surface& dst_surface) {
    // Currently unimplemented. PBO copies should be dropped and we should use a render pass to
    // convert from color to depth and viceversa.
    LOG_WARNING(Render_Vulkan, "Unimplemented");
}

} // namespace Vulkan
