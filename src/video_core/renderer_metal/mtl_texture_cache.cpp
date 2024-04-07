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

Image::Image(TextureCacheRuntime& runtime, const ImageInfo& info, GPUVAddr gpu_addr_,
             VAddr cpu_addr_)
    : VideoCommon::ImageBase(info, gpu_addr_, cpu_addr_) {
    MTL::TextureDescriptor* texture_descriptor = MTL::TextureDescriptor::alloc()->init();
    // TODO: don't hardcode the format
    texture_descriptor->setPixelFormat(MTL::PixelFormatRGBA8Unorm);
    texture_descriptor->setWidth(info.size.width);
    texture_descriptor->setHeight(info.size.height);

    texture = runtime.device.GetDevice()->newTexture(texture_descriptor);
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
    ;
}

void Image::UploadMemory(const StagingBufferRef& map,
                         std::span<const VideoCommon::BufferImageCopy> copies) {
    ;
}

void Image::DownloadMemory(MTL::Buffer* buffer, size_t offset,
                           std::span<const VideoCommon::BufferImageCopy> copies) {
    ;
}

void Image::DownloadMemory(std::span<MTL::Buffer*> buffers, std::span<size_t> offsets,
                           std::span<const VideoCommon::BufferImageCopy> copies) {
    ;
}

void Image::DownloadMemory(const StagingBufferRef& map,
                           std::span<const VideoCommon::BufferImageCopy> copies) {
    ;
}

ImageView::ImageView(TextureCacheRuntime& runtime, const VideoCommon::ImageViewInfo& info,
                     ImageId image_id_, Image& image)
    : VideoCommon::ImageViewBase{info, image.info, image_id_, image.gpu_addr} {
    using Shader::TextureType;
    // TODO: For whatever reason, some images's internal objc objects is NULL
    // TODO: metal-cpp provides no method to check for this
    texture = image.GetHandle()->retain();

    // TODO: create texture view
}

ImageView::ImageView(TextureCacheRuntime& runtime, const VideoCommon::ImageViewInfo& info,
                     ImageId image_id_, Image& image, const SlotVector<Image>& slot_imgs)
    : ImageView(runtime, info, image_id_, image) {
    // TODO: save slot images
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
    texture->release();
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
        color_attachment->setClearColor(MTL::ClearColor::Make(0.5, 1.0, 0.0, 1.0));
        color_attachment->setLoadAction(MTL::LoadActionClear);
        color_attachment->setStoreAction(MTL::StoreActionStore);
        color_attachment->setTexture(color_buffer->GetHandle());
    }
    if (depth_buffer) {
        auto depth_attachment = render_pass->depthAttachment();
        depth_attachment->setClearDepth(1.0);
        depth_attachment->setLoadAction(MTL::LoadActionClear);
        depth_attachment->setStoreAction(MTL::StoreActionStore);
        depth_attachment->setTexture(depth_buffer->GetHandle());
    }
}

} // namespace Metal
