// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "common/common_types.h"
#include "video_core/renderer_vulkan/declarations.h"

namespace Layout {
struct FramebufferLayout;
}

namespace Vulkan {

class VKDevice;
class VKFence;

class VKSwapchain {
public:
    explicit VKSwapchain(vk::SurfaceKHR surface, const VKDevice& device);
    ~VKSwapchain();

    /// Creates (or recreates) the swapchain with a given size.
    void Create(u32 width, u32 height, bool srgb);

    /// Acquires the next image in the swapchain, waits as needed.
    void AcquireNextImage();

    /// Presents the rendered image to the swapchain. Returns true when the swapchains had to be
    /// recreated. Takes responsability for the ownership of fence.
    bool Present(vk::Semaphore render_semaphore, VKFence& fence);

    /// Returns true when the framebuffer layout has changed.
    bool HasFramebufferChanged(const Layout::FramebufferLayout& framebuffer) const;

    const vk::Extent2D& GetSize() const {
        return extent;
    }

    u32 GetImageCount() const {
        return image_count;
    }

    u32 GetImageIndex() const {
        return image_index;
    }

    vk::Image GetImageIndex(u32 index) const {
        return images[index];
    }

    vk::ImageView GetImageViewIndex(u32 index) const {
        return *image_views[index];
    }

    vk::Format GetImageFormat() const {
        return image_format;
    }

    bool GetSrgbState() const {
        return current_srgb;
    }

private:
    void CreateSwapchain(const vk::SurfaceCapabilitiesKHR& capabilities, u32 width, u32 height,
                         bool srgb);
    void CreateSemaphores();
    void CreateImageViews();

    void Destroy();

    const vk::SurfaceKHR surface;
    const VKDevice& device;

    UniqueSwapchainKHR swapchain;

    u32 image_count{};
    std::vector<vk::Image> images;
    std::vector<UniqueImageView> image_views;
    std::vector<UniqueFramebuffer> framebuffers;
    std::vector<VKFence*> fences;
    std::vector<UniqueSemaphore> present_semaphores;

    u32 image_index{};
    u32 frame_index{};

    vk::Format image_format{};
    vk::Extent2D extent{};

    u32 current_width{};
    u32 current_height{};
    bool current_srgb{};
};

} // namespace Vulkan
