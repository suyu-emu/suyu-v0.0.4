// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <vector>

#include "common/assert.h"
#include "common/common_types.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/frontend/emu_window.h"
#include "video_core/gpu.h"
#include "video_core/host1x/gpu_device_memory_manager.h"
#include "video_core/renderer_vulkan/present/filters.h"
#include "video_core/renderer_vulkan/present/fsr.h"
#include "video_core/renderer_vulkan/present/fxaa.h"
#include "video_core/renderer_vulkan/present/smaa.h"
#include "video_core/renderer_vulkan/renderer_vulkan.h"
#include "video_core/renderer_vulkan/vk_blit_screen.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"
#include "video_core/surface.h"
#include "video_core/textures/decoders.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_memory_allocator.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

namespace {

u32 GetBytesPerPixel(const Tegra::FramebufferConfig& framebuffer) {
    using namespace VideoCore::Surface;
    return BytesPerBlock(PixelFormatFromGPUPixelFormat(framebuffer.pixel_format));
}

std::size_t GetSizeInBytes(const Tegra::FramebufferConfig& framebuffer) {
    return static_cast<std::size_t>(framebuffer.stride) *
           static_cast<std::size_t>(framebuffer.height) * GetBytesPerPixel(framebuffer);
}

VkFormat GetFormat(const Tegra::FramebufferConfig& framebuffer) {
    switch (framebuffer.pixel_format) {
    case Service::android::PixelFormat::Rgba8888:
    case Service::android::PixelFormat::Rgbx8888:
        return VK_FORMAT_A8B8G8R8_UNORM_PACK32;
    case Service::android::PixelFormat::Rgb565:
        return VK_FORMAT_R5G6B5_UNORM_PACK16;
    case Service::android::PixelFormat::Bgra8888:
        return VK_FORMAT_B8G8R8A8_UNORM;
    default:
        UNIMPLEMENTED_MSG("Unknown framebuffer pixel format: {}",
                          static_cast<u32>(framebuffer.pixel_format));
        return VK_FORMAT_A8B8G8R8_UNORM_PACK32;
    }
}

} // Anonymous namespace

BlitScreen::BlitScreen(Tegra::MaxwellDeviceMemoryManager& device_memory_, const Device& device_,
                       MemoryAllocator& memory_allocator_, PresentManager& present_manager_,
                       Scheduler& scheduler_)
    : device_memory{device_memory_}, device{device_}, memory_allocator{memory_allocator_},
      present_manager{present_manager_}, scheduler{scheduler_}, image_count{1},
      swapchain_view_format{VK_FORMAT_B8G8R8A8_UNORM} {}

BlitScreen::~BlitScreen() = default;

void BlitScreen::WaitIdle() {
    present_manager.WaitPresent();
    scheduler.Finish();
    device.GetLogical().WaitIdle();
}

void BlitScreen::SetWindowAdaptPass(const Layout::FramebufferLayout& layout) {
    scaling_filter = Settings::values.scaling_filter.GetValue();

    const VkExtent2D adapt_size{
        .width = layout.screen.GetWidth(),
        .height = layout.screen.GetHeight(),
    };

    fsr.reset();

    switch (scaling_filter) {
    case Settings::ScalingFilter::NearestNeighbor:
        window_adapt =
            MakeNearestNeighbor(device, memory_allocator, image_count, swapchain_view_format);
        break;
    case Settings::ScalingFilter::Bicubic:
        window_adapt = MakeBicubic(device, memory_allocator, image_count, swapchain_view_format);
        break;
    case Settings::ScalingFilter::Gaussian:
        window_adapt = MakeGaussian(device, memory_allocator, image_count, swapchain_view_format);
        break;
    case Settings::ScalingFilter::ScaleForce:
        window_adapt = MakeScaleForce(device, memory_allocator, image_count, swapchain_view_format);
        break;
    case Settings::ScalingFilter::Fsr:
        fsr = std::make_unique<FSR>(device, memory_allocator, image_count, adapt_size);
        [[fallthrough]];
    case Settings::ScalingFilter::Bilinear:
    default:
        window_adapt = MakeBilinear(device, memory_allocator, image_count, swapchain_view_format);
        break;
    }
}

void BlitScreen::SetAntiAliasPass() {
    if (anti_alias && anti_aliasing == Settings::values.anti_aliasing.GetValue()) {
        return;
    }

    anti_aliasing = Settings::values.anti_aliasing.GetValue();

    const VkExtent2D render_area{
        .width = Settings::values.resolution_info.ScaleUp(raw_width),
        .height = Settings::values.resolution_info.ScaleUp(raw_height),
    };

    switch (anti_aliasing) {
    case Settings::AntiAliasing::Fxaa:
        anti_alias = std::make_unique<FXAA>(device, memory_allocator, image_count, render_area);
        break;
    case Settings::AntiAliasing::Smaa:
        anti_alias = std::make_unique<SMAA>(device, memory_allocator, image_count, render_area);
        break;
    default:
        anti_alias = std::make_unique<NoAA>();
        break;
    }
}

void BlitScreen::Draw(RasterizerVulkan& rasterizer, const Tegra::FramebufferConfig& framebuffer,
                      const Layout::FramebufferLayout& layout, Frame* dst) {
    const auto texture_info = rasterizer.AccelerateDisplay(
        framebuffer, framebuffer.address + framebuffer.offset, framebuffer.stride);
    const u32 texture_width = texture_info ? texture_info->width : framebuffer.width;
    const u32 texture_height = texture_info ? texture_info->height : framebuffer.height;
    const u32 scaled_width = texture_info ? texture_info->scaled_width : texture_width;
    const u32 scaled_height = texture_info ? texture_info->scaled_height : texture_height;
    const bool use_accelerated = texture_info.has_value();

    RefreshResources(framebuffer);
    SetAntiAliasPass();

    // Finish any pending renderpass
    scheduler.RequestOutsideRenderPassOperationContext();

    scheduler.Wait(resource_ticks[image_index]);
    SCOPE_EXIT({ resource_ticks[image_index] = scheduler.CurrentTick(); });

    VkImage source_image = texture_info ? texture_info->image : *raw_images[image_index];
    VkImageView source_image_view =
        texture_info ? texture_info->image_view : *raw_image_views[image_index];

    const std::span<u8> mapped_span = buffer.Mapped();

    if (!use_accelerated) {
        const u64 image_offset = GetRawImageOffset(framebuffer);

        const DAddr framebuffer_addr = framebuffer.address + framebuffer.offset;
        const u8* const host_ptr = device_memory.GetPointer<u8>(framebuffer_addr);

        // TODO(Rodrigo): Read this from HLE
        constexpr u32 block_height_log2 = 4;
        const u32 bytes_per_pixel = GetBytesPerPixel(framebuffer);
        const u64 linear_size{GetSizeInBytes(framebuffer)};
        const u64 tiled_size{Tegra::Texture::CalculateSize(true, bytes_per_pixel,
                                                           framebuffer.stride, framebuffer.height,
                                                           1, block_height_log2, 0)};
        Tegra::Texture::UnswizzleTexture(
            mapped_span.subspan(image_offset, linear_size), std::span(host_ptr, tiled_size),
            bytes_per_pixel, framebuffer.width, framebuffer.height, 1, block_height_log2, 0);

        const VkBufferImageCopy copy{
            .bufferOffset = image_offset,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            .imageOffset = {.x = 0, .y = 0, .z = 0},
            .imageExtent =
                {
                    .width = framebuffer.width,
                    .height = framebuffer.height,
                    .depth = 1,
                },
        };
        scheduler.Record([this, copy, index = image_index](vk::CommandBuffer cmdbuf) {
            const VkImage image = *raw_images[index];
            const VkImageMemoryBarrier base_barrier{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = nullptr,
                .srcAccessMask = 0,
                .dstAccessMask = 0,
                .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image,
                .subresourceRange{
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            };
            VkImageMemoryBarrier read_barrier = base_barrier;
            read_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            read_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            read_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            VkImageMemoryBarrier write_barrier = base_barrier;
            write_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            write_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                                   read_barrier);
            cmdbuf.CopyBufferToImage(*buffer, image, VK_IMAGE_LAYOUT_GENERAL, copy);
            cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT,
                                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                   0, write_barrier);
        });
    }

    anti_alias->Draw(scheduler, image_index, &source_image, &source_image_view);

    const auto crop_rect = Tegra::NormalizeCrop(framebuffer, texture_width, texture_height);
    const VkExtent2D render_extent{
        .width = scaled_width,
        .height = scaled_height,
    };

    if (fsr) {
        const VkExtent2D adapt_size{
            .width = layout.screen.GetWidth(),
            .height = layout.screen.GetHeight(),
        };

        source_image_view = fsr->Draw(scheduler, image_index, source_image, source_image_view,
                                      render_extent, crop_rect);

        const Common::Rectangle<f32> output_crop{0, 0, 1, 1};
        window_adapt->Draw(scheduler, image_index, source_image_view, adapt_size, output_crop,
                           layout, dst);
    } else {
        window_adapt->Draw(scheduler, image_index, source_image_view, render_extent, crop_rect,
                           layout, dst);
    }
}

void BlitScreen::DrawToFrame(RasterizerVulkan& rasterizer, Frame* frame,
                             const Tegra::FramebufferConfig& framebuffer,
                             const Layout::FramebufferLayout& layout, size_t swapchain_images,
                             VkFormat current_swapchain_view_format) {
    bool resource_update_required = false;
    bool presentation_recreate_required = false;

    // Recreate dynamic resources if the adapting filter changed
    if (!window_adapt || scaling_filter != Settings::values.scaling_filter.GetValue()) {
        resource_update_required = true;
    }

    // Recreate dynamic resources if the the image count or input format changed
    const VkFormat old_framebuffer_format =
        std::exchange(framebuffer_view_format, GetFormat(framebuffer));
    if (swapchain_images != image_count || old_framebuffer_format != framebuffer_view_format) {
        image_count = swapchain_images;
        resource_update_required = true;
    }

    // Recreate the presentation frame if the format or dimensions of the window changed
    const VkFormat old_swapchain_view_format =
        std::exchange(swapchain_view_format, current_swapchain_view_format);
    if (old_swapchain_view_format != current_swapchain_view_format ||
        layout.width != frame->width || layout.height != frame->height) {
        resource_update_required = true;
        presentation_recreate_required = true;
    }

    // If we have a pending resource update, perform it
    if (resource_update_required) {
        // Wait for idle to ensure no resources are in use
        WaitIdle();

        // Set new number of resource ticks
        resource_ticks.resize(swapchain_images);

        // Update window adapt pass
        SetWindowAdaptPass(layout);

        // Update frame format if needed
        if (presentation_recreate_required) {
            present_manager.RecreateFrame(frame, layout.width, layout.height, swapchain_view_format,
                                          window_adapt->GetRenderPass());
        }
    }

    Draw(rasterizer, framebuffer, layout, frame);
    if (++image_index >= image_count) {
        image_index = 0;
    }
}

vk::Framebuffer BlitScreen::CreateFramebuffer(const Layout::FramebufferLayout& layout,
                                              const VkImageView& image_view,
                                              VkFormat current_view_format) {
    const bool format_updated =
        std::exchange(swapchain_view_format, current_view_format) != current_view_format;
    if (!window_adapt || scaling_filter != Settings::values.scaling_filter.GetValue() ||
        format_updated) {
        WaitIdle();
        SetWindowAdaptPass(layout);
    }
    const VkExtent2D extent{
        .width = layout.width,
        .height = layout.height,
    };
    return CreateFramebuffer(image_view, extent, window_adapt->GetRenderPass());
}

vk::Framebuffer BlitScreen::CreateFramebuffer(const VkImageView& image_view, VkExtent2D extent,
                                              VkRenderPass render_pass) {
    return device.GetLogical().CreateFramebuffer(VkFramebufferCreateInfo{
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .renderPass = render_pass,
        .attachmentCount = 1,
        .pAttachments = &image_view,
        .width = extent.width,
        .height = extent.height,
        .layers = 1,
    });
}

void BlitScreen::RefreshResources(const Tegra::FramebufferConfig& framebuffer) {
    if (framebuffer.width == raw_width && framebuffer.height == raw_height &&
        framebuffer.pixel_format == pixel_format && !raw_images.empty()) {
        return;
    }

    raw_width = framebuffer.width;
    raw_height = framebuffer.height;
    pixel_format = framebuffer.pixel_format;
    anti_alias.reset();

    ReleaseRawImages();
    CreateStagingBuffer(framebuffer);
    CreateRawImages(framebuffer);
}

void BlitScreen::ReleaseRawImages() {
    for (const u64 tick : resource_ticks) {
        scheduler.Wait(tick);
    }
    raw_images.clear();
    buffer.reset();
}

void BlitScreen::CreateStagingBuffer(const Tegra::FramebufferConfig& framebuffer) {
    const VkBufferCreateInfo ci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = CalculateBufferSize(framebuffer),
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    };

    buffer = memory_allocator.CreateBuffer(ci, MemoryUsage::Upload);
}

void BlitScreen::CreateRawImages(const Tegra::FramebufferConfig& framebuffer) {
    raw_images.resize(image_count);
    raw_image_views.resize(image_count);

    const auto create_image = [&](bool used_on_framebuffer = false, u32 up_scale = 1,
                                  u32 down_shift = 0) {
        u32 extra_usages = used_on_framebuffer ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                                               : VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        return memory_allocator.CreateImage(VkImageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = used_on_framebuffer ? VK_FORMAT_R16G16B16A16_SFLOAT : framebuffer_view_format,
            .extent =
                {
                    .width = (up_scale * framebuffer.width) >> down_shift,
                    .height = (up_scale * framebuffer.height) >> down_shift,
                    .depth = 1,
                },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = used_on_framebuffer ? VK_IMAGE_TILING_OPTIMAL : VK_IMAGE_TILING_LINEAR,
            .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | extra_usages,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        });
    };
    const auto create_image_view = [&](vk::Image& image, bool used_on_framebuffer = false) {
        return device.GetLogical().CreateImageView(VkImageViewCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = *image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = used_on_framebuffer ? VK_FORMAT_R16G16B16A16_SFLOAT : framebuffer_view_format,
            .components =
                {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                },
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        });
    };

    for (size_t i = 0; i < image_count; ++i) {
        raw_images[i] = create_image();
        raw_image_views[i] = create_image_view(raw_images[i]);
    }
}

u64 BlitScreen::CalculateBufferSize(const Tegra::FramebufferConfig& framebuffer) const {
    return GetSizeInBytes(framebuffer) * image_count;
}

u64 BlitScreen::GetRawImageOffset(const Tegra::FramebufferConfig& framebuffer) const {
    return GetSizeInBytes(framebuffer) * image_index;
}

} // namespace Vulkan
