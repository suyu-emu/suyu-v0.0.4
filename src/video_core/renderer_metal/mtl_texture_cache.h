// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <span>

#include "mtl_staging_buffer_pool.h"
#include "video_core/texture_cache/texture_cache_base.h"

#include "shader_recompiler/shader_info.h"
#include "video_core/renderer_metal/mtl_staging_buffer_pool.h"
#include "video_core/renderer_metal/objc_bridge.h"
#include "video_core/texture_cache/image_view_base.h"

namespace Settings {
struct ResolutionScalingInfo;
}

namespace Metal {

using Common::SlotVector;
using VideoCommon::ImageId;
using VideoCommon::NUM_RT;
using VideoCommon::Region2D;
using VideoCommon::RenderTargets;
using VideoCore::Surface::PixelFormat;

class CommandRecorder;
class Device;
class Image;
class ImageView;
class Framebuffer;

class TextureCacheRuntime {
public:
    explicit TextureCacheRuntime(const Device& device_, CommandRecorder& command_recorder_,
                                 StagingBufferPool& staging_buffer_pool_);

    // TODO: implement
    void Finish() {}

    void TickFrame();

    StagingBufferRef UploadStagingBuffer(size_t size);

    StagingBufferRef DownloadStagingBuffer(size_t size, bool deferred = false);

    void FreeDeferredStagingBuffer(StagingBufferRef& ref);

    bool CanUploadMSAA() const noexcept {
        return true;
    }

    u64 GetDeviceLocalMemory() const {
        return 0;
    }

    u64 GetDeviceMemoryUsage() const {
        return 0;
    }

    bool CanReportMemoryUsage() const {
        return false;
    }

    // TODO: implement
    void BlitImage(Framebuffer* dst_framebuffer, ImageView& dst, ImageView& src,
                   const Region2D& dst_region, const Region2D& src_region,
                   Tegra::Engines::Fermi2D::Filter filter,
                   Tegra::Engines::Fermi2D::Operation operation) {}

    // TODO: implement
    void CopyImage(Image& dst, Image& src, std::span<const VideoCommon::ImageCopy> copies) {}

    // TODO: implement
    void CopyImageMSAA(Image& dst, Image& src, std::span<const VideoCommon::ImageCopy> copies) {}

    bool ShouldReinterpret(Image& dst, Image& src) {
        // HACK
        return false;
    }

    // TODO: implement
    void ReinterpretImage(Image& dst, Image& src, std::span<const VideoCommon::ImageCopy> copies) {}

    // TODO: implement
    void ConvertImage(Framebuffer* dst, ImageView& dst_view, ImageView& src_view) {}

    // TODO: implement
    void InsertUploadMemoryBarrier() {}

    void TransitionImageLayout(Image& image) {}

    // TODO: implement
    void AccelerateImageUpload(Image&, const StagingBufferRef&,
                               std::span<const VideoCommon::SwizzleParameters>) {}

    bool HasNativeBgr() const noexcept {
        return true;
    }

    bool HasBrokenTextureViewFormats() const noexcept {
        return false;
    }

    // TODO: implement
    void BarrierFeedbackLoop() {}

    const Device& device;
    CommandRecorder& command_recorder;
    StagingBufferPool& staging_buffer_pool;
    const Settings::ResolutionScalingInfo& resolution;
};

class Image : public VideoCommon::ImageBase {
public:
    explicit Image(TextureCacheRuntime& runtime, const VideoCommon::ImageInfo& info,
                   GPUVAddr gpu_addr, VAddr cpu_addr);
    explicit Image(const VideoCommon::NullImageParams&);

    ~Image();

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    Image(Image&&) = default;
    Image& operator=(Image&&) = default;

    void UploadMemory(MTLBuffer_t buffer, size_t offset,
                      std::span<const VideoCommon::BufferImageCopy> copies);

    void UploadMemory(const StagingBufferRef& map,
                      std::span<const VideoCommon::BufferImageCopy> copies);

    void DownloadMemory(MTLBuffer_t buffer, size_t offset,
                        std::span<const VideoCommon::BufferImageCopy> copies);

    // For some reason, this function cannot be defined in the .mm file since it would report
    // undefined symbols
    void DownloadMemory(std::span<MTLBuffer_t> buffers, std::span<size_t> offsets,
                        std::span<const VideoCommon::BufferImageCopy> copies) {
        // TODO: implement
    }

    void DownloadMemory(const StagingBufferRef& map,
                        std::span<const VideoCommon::BufferImageCopy> copies);

    bool IsRescaled() const {
        return rescaled;
    }

    bool ScaleUp(bool ignore = false) {
        // HACK
        return true;
    }

    bool ScaleDown(bool ignore = false) {
        // HACK
        return true;
    }

    MTLTexture_t GetHandle() const noexcept {
        return texture;
    }

private:
    MTLTexture_t texture = nil;
    bool initialized = false;

    bool rescaled = false;
};

class ImageView : public VideoCommon::ImageViewBase {
public:
    explicit ImageView(TextureCacheRuntime&, const VideoCommon::ImageViewInfo&, ImageId, Image&);
    explicit ImageView(TextureCacheRuntime&, const VideoCommon::ImageViewInfo&, ImageId, Image&,
                       const SlotVector<Image>&);
    explicit ImageView(TextureCacheRuntime&, const VideoCommon::ImageInfo&,
                       const VideoCommon::ImageViewInfo&, GPUVAddr);
    explicit ImageView(TextureCacheRuntime&, const VideoCommon::NullImageViewParams&);

    ~ImageView();

    ImageView(const ImageView&) = delete;
    ImageView& operator=(const ImageView&) = delete;

    ImageView(ImageView&&) = default;
    ImageView& operator=(ImageView&&) = default;

    MTLTexture_t GetHandle() const noexcept {
        return texture;
    }

private:
    MTLTexture_t texture;
};

class ImageAlloc : public VideoCommon::ImageAllocBase {};

class Sampler {
public:
    explicit Sampler(TextureCacheRuntime&, const Tegra::Texture::TSCEntry&);

    MTLSamplerState_t GetHandle() const noexcept {
        return sampler_state;
    }

private:
    MTLSamplerState_t sampler_state;
};

class Framebuffer {
public:
    explicit Framebuffer(TextureCacheRuntime& runtime, std::span<ImageView*, NUM_RT> color_buffers,
                         ImageView* depth_buffer, const VideoCommon::RenderTargets& key);
    ~Framebuffer();

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    Framebuffer(Framebuffer&&) = default;
    Framebuffer& operator=(Framebuffer&&) = default;

    void CreateRenderPassDescriptor(TextureCacheRuntime& runtime,
                                    std::span<ImageView*, NUM_RT> color_buffers,
                                    ImageView* depth_buffer, bool is_rescaled, size_t width,
                                    size_t height);

    MTLRenderPassDescriptor* GetHandle() const noexcept {
        return render_pass;
    }

private:
    MTLRenderPassDescriptor* render_pass{};
};

struct TextureCacheParams {
    static constexpr bool ENABLE_VALIDATION = true;
    static constexpr bool FRAMEBUFFER_BLITS = false;
    static constexpr bool HAS_EMULATED_COPIES = false;
    static constexpr bool HAS_DEVICE_MEMORY_INFO = true;
    static constexpr bool IMPLEMENTS_ASYNC_DOWNLOADS = true;

    using Runtime = Metal::TextureCacheRuntime;
    using Image = Metal::Image;
    using ImageAlloc = Metal::ImageAlloc;
    using ImageView = Metal::ImageView;
    using Sampler = Metal::Sampler;
    using Framebuffer = Metal::Framebuffer;
    using AsyncBuffer = Metal::StagingBufferRef;
    using BufferType = MTLBuffer_t;
};

using TextureCache = VideoCommon::TextureCache<TextureCacheParams>;

} // namespace Metal
