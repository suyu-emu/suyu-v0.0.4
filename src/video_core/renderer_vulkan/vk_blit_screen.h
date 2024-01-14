// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "core/frontend/framebuffer_layout.h"
#include "video_core/host1x/gpu_device_memory_manager.h"
#include "video_core/vulkan_common/vulkan_memory_allocator.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Core {
class System;
}

namespace Core::Frontend {
class EmuWindow;
}

namespace Tegra {
struct FramebufferConfig;
}

namespace VideoCore {
class RasterizerInterface;
}

namespace Service::android {
enum class PixelFormat : u32;
}

namespace Settings {
enum class AntiAliasing : u32;
enum class ScalingFilter : u32;
} // namespace Settings

namespace Vulkan {

class AntiAliasPass;
class Device;
class FSR;
class RasterizerVulkan;
class Scheduler;
class PresentManager;
class WindowAdaptPass;

struct Frame;

struct FramebufferTextureInfo {
    VkImage image{};
    VkImageView image_view{};
    u32 width{};
    u32 height{};
    u32 scaled_width{};
    u32 scaled_height{};
};

class BlitScreen {
public:
    explicit BlitScreen(Tegra::MaxwellDeviceMemoryManager& device_memory, const Device& device,
                        MemoryAllocator& memory_allocator, PresentManager& present_manager,
                        Scheduler& scheduler);
    ~BlitScreen();

    void DrawToFrame(RasterizerVulkan& rasterizer, Frame* frame,
                     const Tegra::FramebufferConfig& framebuffer,
                     const Layout::FramebufferLayout& layout, size_t swapchain_images,
                     VkFormat current_swapchain_view_format);

    [[nodiscard]] vk::Framebuffer CreateFramebuffer(const Layout::FramebufferLayout& layout,
                                                    const VkImageView& image_view,
                                                    VkFormat current_view_format);

private:
    void WaitIdle();
    void SetWindowAdaptPass(const Layout::FramebufferLayout& layout);
    void SetAntiAliasPass();

    void Draw(RasterizerVulkan& rasterizer, const Tegra::FramebufferConfig& framebuffer,
              const Layout::FramebufferLayout& layout, Frame* dst);

    vk::Framebuffer CreateFramebuffer(const VkImageView& image_view, VkExtent2D extent,
                                      VkRenderPass render_pass);

    void RefreshResources(const Tegra::FramebufferConfig& framebuffer);
    void ReleaseRawImages();
    void CreateStagingBuffer(const Tegra::FramebufferConfig& framebuffer);
    void CreateRawImages(const Tegra::FramebufferConfig& framebuffer);

    u64 CalculateBufferSize(const Tegra::FramebufferConfig& framebuffer) const;
    u64 GetRawImageOffset(const Tegra::FramebufferConfig& framebuffer) const;

    Tegra::MaxwellDeviceMemoryManager& device_memory;
    const Device& device;
    MemoryAllocator& memory_allocator;
    PresentManager& present_manager;
    Scheduler& scheduler;
    std::size_t image_count;
    std::size_t image_index{};

    vk::Buffer buffer;

    std::vector<u64> resource_ticks;

    std::vector<vk::Image> raw_images;
    std::vector<vk::ImageView> raw_image_views;
    u32 raw_width = 0;
    u32 raw_height = 0;

    Service::android::PixelFormat pixel_format{};
    VkFormat framebuffer_view_format;
    VkFormat swapchain_view_format;

    Settings::AntiAliasing anti_aliasing{};
    Settings::ScalingFilter scaling_filter{};

    std::unique_ptr<FSR> fsr;
    std::unique_ptr<AntiAliasPass> anti_alias;
    std::unique_ptr<WindowAdaptPass> window_adapt;
};

} // namespace Vulkan
