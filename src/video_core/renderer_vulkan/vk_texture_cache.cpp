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

#include "common/alignment.h"
#include "common/assert.h"
#include "common/common_types.h"
#include "core/core.h"
#include "core/memory.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/morton.h"
#include "video_core/renderer_vulkan/declarations.h"
#include "video_core/renderer_vulkan/maxwell_to_vk.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_memory_manager.h"
#include "video_core/renderer_vulkan/vk_rasterizer.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_staging_buffer_pool.h"
#include "video_core/renderer_vulkan/vk_texture_cache.h"
#include "video_core/surface.h"
#include "video_core/textures/convert.h"

namespace Vulkan {

using VideoCore::MortonSwizzle;
using VideoCore::MortonSwizzleMode;

using Tegra::Texture::SwizzleSource;
using VideoCore::Surface::PixelFormat;
using VideoCore::Surface::SurfaceCompression;
using VideoCore::Surface::SurfaceTarget;

namespace {

vk::ImageType SurfaceTargetToImage(SurfaceTarget target) {
    switch (target) {
    case SurfaceTarget::Texture1D:
    case SurfaceTarget::Texture1DArray:
        return vk::ImageType::e1D;
    case SurfaceTarget::Texture2D:
    case SurfaceTarget::Texture2DArray:
    case SurfaceTarget::TextureCubemap:
    case SurfaceTarget::TextureCubeArray:
        return vk::ImageType::e2D;
    case SurfaceTarget::Texture3D:
        return vk::ImageType::e3D;
    case SurfaceTarget::TextureBuffer:
        UNREACHABLE();
        return {};
    }
    UNREACHABLE_MSG("Unknown texture target={}", static_cast<u32>(target));
    return {};
}

vk::ImageAspectFlags PixelFormatToImageAspect(PixelFormat pixel_format) {
    if (pixel_format < PixelFormat::MaxColorFormat) {
        return vk::ImageAspectFlagBits::eColor;
    } else if (pixel_format < PixelFormat::MaxDepthFormat) {
        return vk::ImageAspectFlagBits::eDepth;
    } else if (pixel_format < PixelFormat::MaxDepthStencilFormat) {
        return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
    } else {
        UNREACHABLE_MSG("Invalid pixel format={}", static_cast<u32>(pixel_format));
        return vk::ImageAspectFlagBits::eColor;
    }
}

vk::ImageViewType GetImageViewType(SurfaceTarget target) {
    switch (target) {
    case SurfaceTarget::Texture1D:
        return vk::ImageViewType::e1D;
    case SurfaceTarget::Texture2D:
        return vk::ImageViewType::e2D;
    case SurfaceTarget::Texture3D:
        return vk::ImageViewType::e3D;
    case SurfaceTarget::Texture1DArray:
        return vk::ImageViewType::e1DArray;
    case SurfaceTarget::Texture2DArray:
        return vk::ImageViewType::e2DArray;
    case SurfaceTarget::TextureCubemap:
        return vk::ImageViewType::eCube;
    case SurfaceTarget::TextureCubeArray:
        return vk::ImageViewType::eCubeArray;
    case SurfaceTarget::TextureBuffer:
        break;
    }
    UNREACHABLE();
    return {};
}

UniqueBuffer CreateBuffer(const VKDevice& device, const SurfaceParams& params) {
    // TODO(Rodrigo): Move texture buffer creation to the buffer cache
    const vk::BufferCreateInfo buffer_ci({}, params.GetHostSizeInBytes(),
                                         vk::BufferUsageFlagBits::eUniformTexelBuffer |
                                             vk::BufferUsageFlagBits::eTransferSrc |
                                             vk::BufferUsageFlagBits::eTransferDst,
                                         vk::SharingMode::eExclusive, 0, nullptr);
    const auto dev = device.GetLogical();
    const auto& dld = device.GetDispatchLoader();
    return dev.createBufferUnique(buffer_ci, nullptr, dld);
}

vk::BufferViewCreateInfo GenerateBufferViewCreateInfo(const VKDevice& device,
                                                      const SurfaceParams& params,
                                                      vk::Buffer buffer) {
    ASSERT(params.IsBuffer());

    const auto format =
        MaxwellToVK::SurfaceFormat(device, FormatType::Buffer, params.pixel_format).format;
    return vk::BufferViewCreateInfo({}, buffer, format, 0, params.GetHostSizeInBytes());
}

vk::ImageCreateInfo GenerateImageCreateInfo(const VKDevice& device, const SurfaceParams& params) {
    constexpr auto sample_count = vk::SampleCountFlagBits::e1;
    constexpr auto tiling = vk::ImageTiling::eOptimal;

    ASSERT(!params.IsBuffer());

    const auto [format, attachable, storage] =
        MaxwellToVK::SurfaceFormat(device, FormatType::Optimal, params.pixel_format);

    auto image_usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst |
                       vk::ImageUsageFlagBits::eTransferSrc;
    if (attachable) {
        image_usage |= params.IsPixelFormatZeta() ? vk::ImageUsageFlagBits::eDepthStencilAttachment
                                                  : vk::ImageUsageFlagBits::eColorAttachment;
    }
    if (storage) {
        image_usage |= vk::ImageUsageFlagBits::eStorage;
    }

    vk::ImageCreateFlags flags;
    vk::Extent3D extent;
    switch (params.target) {
    case SurfaceTarget::TextureCubemap:
    case SurfaceTarget::TextureCubeArray:
        flags |= vk::ImageCreateFlagBits::eCubeCompatible;
        [[fallthrough]];
    case SurfaceTarget::Texture1D:
    case SurfaceTarget::Texture1DArray:
    case SurfaceTarget::Texture2D:
    case SurfaceTarget::Texture2DArray:
        extent = vk::Extent3D(params.width, params.height, 1);
        break;
    case SurfaceTarget::Texture3D:
        extent = vk::Extent3D(params.width, params.height, params.depth);
        break;
    case SurfaceTarget::TextureBuffer:
        UNREACHABLE();
    }

    return vk::ImageCreateInfo(flags, SurfaceTargetToImage(params.target), format, extent,
                               params.num_levels, static_cast<u32>(params.GetNumLayers()),
                               sample_count, tiling, image_usage, vk::SharingMode::eExclusive, 0,
                               nullptr, vk::ImageLayout::eUndefined);
}

} // Anonymous namespace

CachedSurface::CachedSurface(Core::System& system, const VKDevice& device,
                             VKResourceManager& resource_manager, VKMemoryManager& memory_manager,
                             VKScheduler& scheduler, VKStagingBufferPool& staging_pool,
                             GPUVAddr gpu_addr, const SurfaceParams& params)
    : SurfaceBase<View>{gpu_addr, params}, system{system}, device{device},
      resource_manager{resource_manager}, memory_manager{memory_manager}, scheduler{scheduler},
      staging_pool{staging_pool} {
    if (params.IsBuffer()) {
        buffer = CreateBuffer(device, params);
        commit = memory_manager.Commit(*buffer, false);

        const auto buffer_view_ci = GenerateBufferViewCreateInfo(device, params, *buffer);
        format = buffer_view_ci.format;

        const auto dev = device.GetLogical();
        const auto& dld = device.GetDispatchLoader();
        buffer_view = dev.createBufferViewUnique(buffer_view_ci, nullptr, dld);
    } else {
        const auto image_ci = GenerateImageCreateInfo(device, params);
        format = image_ci.format;

        image.emplace(device, scheduler, image_ci, PixelFormatToImageAspect(params.pixel_format));
        commit = memory_manager.Commit(image->GetHandle(), false);
    }

    // TODO(Rodrigo): Move this to a virtual function.
    main_view = CreateViewInner(
        ViewParams(params.target, 0, static_cast<u32>(params.GetNumLayers()), 0, params.num_levels),
        true);
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

    FullTransition(vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferRead,
                   vk::ImageLayout::eTransferSrcOptimal);

    const auto& buffer = staging_pool.GetUnusedBuffer(host_memory_size, true);
    // TODO(Rodrigo): Do this in a single copy
    for (u32 level = 0; level < params.num_levels; ++level) {
        scheduler.Record([image = image->GetHandle(), buffer = *buffer.handle,
                          copy = GetBufferImageCopy(level)](auto cmdbuf, auto& dld) {
            cmdbuf.copyImageToBuffer(image, vk::ImageLayout::eTransferSrcOptimal, buffer, {copy},
                                     dld);
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
    return CreateViewInner(params, false);
}

View CachedSurface::CreateViewInner(const ViewParams& params, bool is_proxy) {
    // TODO(Rodrigo): Add name decorations
    return views[params] = std::make_shared<CachedSurfaceView>(device, *this, params, is_proxy);
}

void CachedSurface::UploadBuffer(const std::vector<u8>& staging_buffer) {
    const auto& src_buffer = staging_pool.GetUnusedBuffer(host_memory_size, true);
    std::memcpy(src_buffer.commit->Map(host_memory_size), staging_buffer.data(), host_memory_size);

    scheduler.Record([src_buffer = *src_buffer.handle, dst_buffer = *buffer,
                      size = params.GetHostSizeInBytes()](auto cmdbuf, auto& dld) {
        const vk::BufferCopy copy(0, 0, size);
        cmdbuf.copyBuffer(src_buffer, dst_buffer, {copy}, dld);

        cmdbuf.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eVertexShader, {}, {},
            {vk::BufferMemoryBarrier(vk::AccessFlagBits::eTransferWrite,
                                     vk::AccessFlagBits::eShaderRead, 0, 0, dst_buffer, 0, size)},
            {}, dld);
    });
}

void CachedSurface::UploadImage(const std::vector<u8>& staging_buffer) {
    const auto& src_buffer = staging_pool.GetUnusedBuffer(host_memory_size, true);
    std::memcpy(src_buffer.commit->Map(host_memory_size), staging_buffer.data(), host_memory_size);

    FullTransition(vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferWrite,
                   vk::ImageLayout::eTransferDstOptimal);

    for (u32 level = 0; level < params.num_levels; ++level) {
        vk::BufferImageCopy copy = GetBufferImageCopy(level);
        if (image->GetAspectMask() ==
            (vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil)) {
            vk::BufferImageCopy depth = copy;
            vk::BufferImageCopy stencil = copy;
            depth.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eDepth;
            stencil.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eStencil;
            scheduler.Record([buffer = *src_buffer.handle, image = image->GetHandle(), depth,
                              stencil](auto cmdbuf, auto& dld) {
                cmdbuf.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal,
                                         {depth, stencil}, dld);
            });
        } else {
            scheduler.Record([buffer = *src_buffer.handle, image = image->GetHandle(),
                              copy](auto cmdbuf, auto& dld) {
                cmdbuf.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal,
                                         {copy}, dld);
            });
        }
    }
}

vk::BufferImageCopy CachedSurface::GetBufferImageCopy(u32 level) const {
    const u32 vk_depth = params.target == SurfaceTarget::Texture3D ? params.GetMipDepth(level) : 1;
    const auto compression_type = params.GetCompressionType();
    const std::size_t mip_offset = compression_type == SurfaceCompression::Converted
                                       ? params.GetConvertedMipmapOffset(level)
                                       : params.GetHostMipmapLevelOffset(level);

    return vk::BufferImageCopy(
        mip_offset, 0, 0,
        {image->GetAspectMask(), level, 0, static_cast<u32>(params.GetNumLayers())}, {0, 0, 0},
        {params.GetMipWidth(level), params.GetMipHeight(level), vk_depth});
}

vk::ImageSubresourceRange CachedSurface::GetImageSubresourceRange() const {
    return {image->GetAspectMask(), 0, params.num_levels, 0,
            static_cast<u32>(params.GetNumLayers())};
}

CachedSurfaceView::CachedSurfaceView(const VKDevice& device, CachedSurface& surface,
                                     const ViewParams& params, bool is_proxy)
    : VideoCommon::ViewBase{params}, params{surface.GetSurfaceParams()},
      image{surface.GetImageHandle()}, buffer_view{surface.GetBufferViewHandle()},
      aspect_mask{surface.GetAspectMask()}, device{device}, surface{surface},
      base_layer{params.base_layer}, num_layers{params.num_layers}, base_level{params.base_level},
      num_levels{params.num_levels}, image_view_type{image ? GetImageViewType(params.target)
                                                           : vk::ImageViewType{}} {}

CachedSurfaceView::~CachedSurfaceView() = default;

vk::ImageView CachedSurfaceView::GetHandle(SwizzleSource x_source, SwizzleSource y_source,
                                           SwizzleSource z_source, SwizzleSource w_source) {
    const u32 swizzle = EncodeSwizzle(x_source, y_source, z_source, w_source);
    if (last_image_view && last_swizzle == swizzle) {
        return last_image_view;
    }
    last_swizzle = swizzle;

    const auto [entry, is_cache_miss] = view_cache.try_emplace(swizzle);
    auto& image_view = entry->second;
    if (!is_cache_miss) {
        return last_image_view = *image_view;
    }

    auto swizzle_x = MaxwellToVK::SwizzleSource(x_source);
    auto swizzle_y = MaxwellToVK::SwizzleSource(y_source);
    auto swizzle_z = MaxwellToVK::SwizzleSource(z_source);
    auto swizzle_w = MaxwellToVK::SwizzleSource(w_source);

    if (params.pixel_format == VideoCore::Surface::PixelFormat::A1B5G5R5U) {
        // A1B5G5R5 is implemented as A1R5G5B5, we have to change the swizzle here.
        std::swap(swizzle_x, swizzle_z);
    }

    // Games can sample depth or stencil values on textures. This is decided by the swizzle value on
    // hardware. To emulate this on Vulkan we specify it in the aspect.
    vk::ImageAspectFlags aspect = aspect_mask;
    if (aspect == (vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil)) {
        UNIMPLEMENTED_IF(x_source != SwizzleSource::R && x_source != SwizzleSource::G);
        const bool is_first = x_source == SwizzleSource::R;
        switch (params.pixel_format) {
        case VideoCore::Surface::PixelFormat::Z24S8:
        case VideoCore::Surface::PixelFormat::Z32FS8:
            aspect = is_first ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eStencil;
            break;
        case VideoCore::Surface::PixelFormat::S8Z24:
            aspect = is_first ? vk::ImageAspectFlagBits::eStencil : vk::ImageAspectFlagBits::eDepth;
            break;
        default:
            aspect = vk::ImageAspectFlagBits::eDepth;
            UNIMPLEMENTED();
        }

        // Vulkan doesn't seem to understand swizzling of a depth stencil image, use identity
        swizzle_x = vk::ComponentSwizzle::eR;
        swizzle_y = vk::ComponentSwizzle::eG;
        swizzle_z = vk::ComponentSwizzle::eB;
        swizzle_w = vk::ComponentSwizzle::eA;
    }

    const vk::ImageViewCreateInfo image_view_ci(
        {}, surface.GetImageHandle(), image_view_type, surface.GetImage().GetFormat(),
        {swizzle_x, swizzle_y, swizzle_z, swizzle_w},
        {aspect, base_level, num_levels, base_layer, num_layers});

    const auto dev = device.GetLogical();
    image_view = dev.createImageViewUnique(image_view_ci, nullptr, device.GetDispatchLoader());
    return last_image_view = *image_view;
}

VKTextureCache::VKTextureCache(Core::System& system, VideoCore::RasterizerInterface& rasterizer,
                               const VKDevice& device, VKResourceManager& resource_manager,
                               VKMemoryManager& memory_manager, VKScheduler& scheduler,
                               VKStagingBufferPool& staging_pool)
    : TextureCache(system, rasterizer), device{device}, resource_manager{resource_manager},
      memory_manager{memory_manager}, scheduler{scheduler}, staging_pool{staging_pool} {}

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
                            vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferRead,
                            vk::ImageLayout::eTransferSrcOptimal);
    dst_surface->Transition(
        dst_base_layer, num_layers, copy_params.dest_level, 1, vk::PipelineStageFlagBits::eTransfer,
        vk::AccessFlagBits::eTransferWrite, vk::ImageLayout::eTransferDstOptimal);

    const vk::ImageSubresourceLayers src_subresource(
        src_surface->GetAspectMask(), copy_params.source_level, copy_params.source_z, num_layers);
    const vk::ImageSubresourceLayers dst_subresource(
        dst_surface->GetAspectMask(), copy_params.dest_level, dst_base_layer, num_layers);
    const vk::Offset3D src_offset(copy_params.source_x, copy_params.source_y, 0);
    const vk::Offset3D dst_offset(copy_params.dest_x, copy_params.dest_y, dst_offset_z);
    const vk::Extent3D extent(copy_params.width, copy_params.height, extent_z);
    const vk::ImageCopy copy(src_subresource, src_offset, dst_subresource, dst_offset, extent);
    const vk::Image src_image = src_surface->GetImageHandle();
    const vk::Image dst_image = dst_surface->GetImageHandle();
    scheduler.Record([src_image, dst_image, copy](auto cmdbuf, auto& dld) {
        cmdbuf.copyImage(src_image, vk::ImageLayout::eTransferSrcOptimal, dst_image,
                         vk::ImageLayout::eTransferDstOptimal, {copy}, dld);
    });
}

void VKTextureCache::ImageBlit(View& src_view, View& dst_view,
                               const Tegra::Engines::Fermi2D::Config& copy_config) {
    // We can't blit inside a renderpass
    scheduler.RequestOutsideRenderPassOperationContext();

    src_view->Transition(vk::ImageLayout::eTransferSrcOptimal, vk::PipelineStageFlagBits::eTransfer,
                         vk::AccessFlagBits::eTransferRead);
    dst_view->Transition(vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits::eTransfer,
                         vk::AccessFlagBits::eTransferWrite);

    const auto& cfg = copy_config;
    const auto src_top_left = vk::Offset3D(cfg.src_rect.left, cfg.src_rect.top, 0);
    const auto src_bot_right = vk::Offset3D(cfg.src_rect.right, cfg.src_rect.bottom, 1);
    const auto dst_top_left = vk::Offset3D(cfg.dst_rect.left, cfg.dst_rect.top, 0);
    const auto dst_bot_right = vk::Offset3D(cfg.dst_rect.right, cfg.dst_rect.bottom, 1);
    const vk::ImageBlit blit(src_view->GetImageSubresourceLayers(), {src_top_left, src_bot_right},
                             dst_view->GetImageSubresourceLayers(), {dst_top_left, dst_bot_right});
    const bool is_linear = copy_config.filter == Tegra::Engines::Fermi2D::Filter::Linear;

    scheduler.Record([src_image = src_view->GetImage(), dst_image = dst_view->GetImage(), blit,
                      is_linear](auto cmdbuf, auto& dld) {
        cmdbuf.blitImage(src_image, vk::ImageLayout::eTransferSrcOptimal, dst_image,
                         vk::ImageLayout::eTransferDstOptimal, {blit},
                         is_linear ? vk::Filter::eLinear : vk::Filter::eNearest, dld);
    });
}

void VKTextureCache::BufferCopy(Surface& src_surface, Surface& dst_surface) {
    // Currently unimplemented. PBO copies should be dropped and we should use a render pass to
    // convert from color to depth and viceversa.
    LOG_WARNING(Render_Vulkan, "Unimplemented");
}

} // namespace Vulkan
