// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <span>
#include <vector>

#include <boost/container/small_vector.hpp>

#include "common/bit_cast.h"
#include "common/bit_util.h"
#include "common/settings.h"

#include "video_core/renderer_metal/maxwell_to_mtl.h"
#include "video_core/renderer_metal/mtl_command_recorder.h"
#include "video_core/renderer_metal/mtl_device.h"
#include "video_core/renderer_metal/mtl_texture_cache.h"

#include "video_core/engines/fermi_2d.h"
#include "video_core/texture_cache/formatter.h"
#include "video_core/texture_cache/samples_helper.h"
#include "video_core/texture_cache/util.h"

namespace Metal {

using Tegra::Engines::Fermi2D;
using Tegra::Texture::SwizzleSource;
using Tegra::Texture::TextureMipmapFilter;
using VideoCommon::BufferImageCopy;
using VideoCommon::ImageFlagBits;
using VideoCommon::ImageInfo;
using VideoCommon::ImageType;
using VideoCommon::SubresourceRange;
using VideoCore::Surface::BytesPerBlock;
using VideoCore::Surface::IsPixelFormatASTC;
using VideoCore::Surface::IsPixelFormatInteger;
using VideoCore::Surface::SurfaceType;

TextureCacheRuntime::TextureCacheRuntime(const Device& device_, CommandRecorder& command_recorder_,
                                         StagingBufferPool& staging_buffer_pool_)
    : device{device_}, command_recorder{command_recorder_},
      staging_buffer_pool{staging_buffer_pool_}, resolution{Settings::values.resolution_info} {}

void TextureCacheRuntime::TickFrame() {}

StagingBufferRef TextureCacheRuntime::UploadStagingBuffer(size_t size) {
    return staging_buffer_pool.Request(size, MemoryUsage::Upload);
}

StagingBufferRef TextureCacheRuntime::DownloadStagingBuffer(size_t size, bool deferred) {
    return staging_buffer_pool.Request(size, MemoryUsage::Download, deferred);
}

void TextureCacheRuntime::FreeDeferredStagingBuffer(StagingBufferRef& ref) {
    staging_buffer_pool.FreeDeferred(ref);
}

Image::Image(TextureCacheRuntime& runtime_, const ImageInfo& info, GPUVAddr gpu_addr_,
             VAddr cpu_addr_)
    : VideoCommon::ImageBase(info, gpu_addr_, cpu_addr_), runtime{&runtime_} {
    const auto& pixel_format_info = MaxwellToMTL::GetPixelFormatInfo(info.format);

    MTL::TextureDescriptor* texture_descriptor = MTL::TextureDescriptor::alloc()->init();
    texture_descriptor->setPixelFormat(pixel_format_info.pixel_format);
    texture_descriptor->setWidth(info.size.width);
    texture_descriptor->setHeight(info.size.height);
    texture_descriptor->setDepth(info.size.depth);

    MTL::TextureUsage usage = MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite;
    if (pixel_format_info.can_be_render_target) {
        usage |= MTL::TextureUsageRenderTarget;
    }
    texture_descriptor->setUsage(usage);

    texture = runtime->device.GetDevice()->newTexture(texture_descriptor);
}

Image::Image(const VideoCommon::NullImageParams& params) : VideoCommon::ImageBase{params} {}

Image::~Image() {
    if (texture) {
        texture->release();
    }
}

// TODO: implement these
void Image::UploadMemory(MTL::Buffer* buffer, size_t offset,
                         std::span<const VideoCommon::BufferImageCopy> copies) {
    for (const VideoCommon::BufferImageCopy& copy : copies) {
        size_t bytes_per_row = MaxwellToMTL::GetTextureBytesPerRow(info.format, info.size.width);
        size_t bytes_per_image = info.size.height * bytes_per_row;
        MTL::Size size = MTL::Size::Make(info.size.width, info.size.height, 1);
        MTL::Origin origin = MTL::Origin::Make(copy.image_offset.x, copy.image_offset.y,
                                               copy.image_subresource.base_layer);
        runtime->command_recorder.GetBlitCommandEncoder()->copyFromBuffer(
            buffer, offset, bytes_per_row, bytes_per_image, size, texture, 0, 0, origin);
    }
}

void Image::UploadMemory(const StagingBufferRef& map,
                         std::span<const VideoCommon::BufferImageCopy> copies) {
    UploadMemory(map.buffer, map.offset, copies);
}

void Image::DownloadMemory(MTL::Buffer* buffer, size_t offset,
                           std::span<const VideoCommon::BufferImageCopy> copies) {
    LOG_DEBUG(Render_Metal, "called");
}

void Image::DownloadMemory(std::span<MTL::Buffer*> buffers, std::span<size_t> offsets,
                           std::span<const VideoCommon::BufferImageCopy> copies) {
    LOG_DEBUG(Render_Metal, "called");
}

void Image::DownloadMemory(const StagingBufferRef& map,
                           std::span<const VideoCommon::BufferImageCopy> copies) {
    LOG_DEBUG(Render_Metal, "called");
}

ImageView::ImageView(TextureCacheRuntime& runtime, const VideoCommon::ImageViewInfo& info,
                     ImageId image_id_, Image& image)
    : VideoCommon::ImageViewBase{info, image.info, image_id_, image.gpu_addr} {
    using Shader::TextureType;
    texture = image.GetHandle();

    // TODO: implement
}

// TODO: save slot images
ImageView::ImageView(TextureCacheRuntime& runtime, const VideoCommon::ImageViewInfo& info,
                     ImageId image_id_, Image& image, const SlotVector<Image>& slot_imgs)
    : ImageView(runtime, info, image_id_, image) {
    texture = image.GetHandle();
}

ImageView::ImageView(TextureCacheRuntime&, const VideoCommon::ImageInfo& info,
                     const VideoCommon::ImageViewInfo& view_info, GPUVAddr gpu_addr_)
    : VideoCommon::ImageViewBase{info, view_info, gpu_addr_} {
    // TODO: implement
}

ImageView::ImageView(TextureCacheRuntime& runtime, const VideoCommon::NullImageViewParams& params)
    : VideoCommon::ImageViewBase{params} {
    // TODO: implement
}

ImageView::~ImageView() {
    // TODO: uncomment
    // texture->release();
}

Sampler::Sampler(TextureCacheRuntime& runtime, const Tegra::Texture::TSCEntry& tsc) {
    MTL::SamplerDescriptor* sampler_descriptor = MTL::SamplerDescriptor::alloc()->init();

    // TODO: configure the descriptor

    sampler_state = runtime.device.GetDevice()->newSamplerState(sampler_descriptor);
}

Framebuffer::Framebuffer(TextureCacheRuntime& runtime, std::span<ImageView*, NUM_RT> color_buffers,
                         ImageView* depth_buffer, const VideoCommon::RenderTargets& key) {
    CreateRenderPassDescriptor(runtime, color_buffers, depth_buffer, key.is_rescaled,
                               key.size.width, key.size.height);
}

Framebuffer::~Framebuffer() = default;

void Framebuffer::CreateRenderPassDescriptor(TextureCacheRuntime& runtime,
                                             std::span<ImageView*, NUM_RT> color_buffers,
                                             ImageView* depth_buffer, bool is_rescaled,
                                             size_t width, size_t height) {
    render_pass = MTL::RenderPassDescriptor::alloc()->init();

    for (size_t index = 0; index < NUM_RT; ++index) {
        const ImageView* const color_buffer = color_buffers[index];
        if (!color_buffer) {
            continue;
        }
        // TODO: don't use index as attachment index
        auto color_attachment = render_pass->colorAttachments()->object(index);
        color_attachment->setLoadAction(MTL::LoadActionLoad);
        color_attachment->setStoreAction(MTL::StoreActionStore);
        color_attachment->setTexture(color_buffer->GetHandle());
    }
    if (depth_buffer) {
        auto depth_attachment = render_pass->depthAttachment();
        depth_attachment->setLoadAction(MTL::LoadActionLoad);
        depth_attachment->setStoreAction(MTL::StoreActionStore);
        depth_attachment->setTexture(depth_buffer->GetHandle());
    }
}

} // namespace Metal
