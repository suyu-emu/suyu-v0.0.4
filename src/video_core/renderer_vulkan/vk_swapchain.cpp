// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <limits>
#include <vector>

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/frontend/framebuffer_layout.h"
#include "video_core/renderer_vulkan/declarations.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_resource_manager.h"
#include "video_core/renderer_vulkan/vk_swapchain.h"

namespace Vulkan {

namespace {

vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats,
                                             bool srgb) {
    if (formats.size() == 1 && formats[0].format == vk::Format::eUndefined) {
        vk::SurfaceFormatKHR format;
        format.format = vk::Format::eB8G8R8A8Unorm;
        format.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
        return format;
    }
    const auto& found = std::find_if(formats.begin(), formats.end(), [srgb](const auto& format) {
        const auto request_format = srgb ? vk::Format::eB8G8R8A8Srgb : vk::Format::eB8G8R8A8Unorm;
        return format.format == request_format &&
               format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
    });
    return found != formats.end() ? *found : formats[0];
}

vk::PresentModeKHR ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& modes) {
    // Mailbox doesn't lock the application like fifo (vsync), prefer it
    const auto& found = std::find_if(modes.begin(), modes.end(), [](const auto& mode) {
        return mode == vk::PresentModeKHR::eMailbox;
    });
    return found != modes.end() ? *found : vk::PresentModeKHR::eFifo;
}

vk::Extent2D ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, u32 width,
                              u32 height) {
    constexpr auto undefined_size{std::numeric_limits<u32>::max()};
    if (capabilities.currentExtent.width != undefined_size) {
        return capabilities.currentExtent;
    }
    vk::Extent2D extent = {width, height};
    extent.width = std::max(capabilities.minImageExtent.width,
                            std::min(capabilities.maxImageExtent.width, extent.width));
    extent.height = std::max(capabilities.minImageExtent.height,
                             std::min(capabilities.maxImageExtent.height, extent.height));
    return extent;
}

} // Anonymous namespace

VKSwapchain::VKSwapchain(vk::SurfaceKHR surface, const VKDevice& device)
    : surface{surface}, device{device} {}

VKSwapchain::~VKSwapchain() = default;

void VKSwapchain::Create(u32 width, u32 height, bool srgb) {
    const auto& dld = device.GetDispatchLoader();
    const auto physical_device = device.GetPhysical();
    const auto capabilities{physical_device.getSurfaceCapabilitiesKHR(surface, dld)};
    if (capabilities.maxImageExtent.width == 0 || capabilities.maxImageExtent.height == 0) {
        return;
    }

    device.GetLogical().waitIdle(dld);
    Destroy();

    CreateSwapchain(capabilities, width, height, srgb);
    CreateSemaphores();
    CreateImageViews();

    fences.resize(image_count, nullptr);
}

void VKSwapchain::AcquireNextImage() {
    const auto dev{device.GetLogical()};
    const auto& dld{device.GetDispatchLoader()};
    dev.acquireNextImageKHR(*swapchain, std::numeric_limits<u64>::max(),
                            *present_semaphores[frame_index], {}, &image_index, dld);

    if (auto& fence = fences[image_index]; fence) {
        fence->Wait();
        fence->Release();
        fence = nullptr;
    }
}

bool VKSwapchain::Present(vk::Semaphore render_semaphore, VKFence& fence) {
    const vk::Semaphore present_semaphore{*present_semaphores[frame_index]};
    const std::array<vk::Semaphore, 2> semaphores{present_semaphore, render_semaphore};
    const u32 wait_semaphore_count{render_semaphore ? 2U : 1U};
    const auto& dld{device.GetDispatchLoader()};
    const auto present_queue{device.GetPresentQueue()};
    bool recreated = false;

    const vk::PresentInfoKHR present_info(wait_semaphore_count, semaphores.data(), 1,
                                          &swapchain.get(), &image_index, {});
    switch (const auto result = present_queue.presentKHR(&present_info, dld); result) {
    case vk::Result::eSuccess:
        break;
    case vk::Result::eErrorOutOfDateKHR:
        if (current_width > 0 && current_height > 0) {
            Create(current_width, current_height, current_srgb);
            recreated = true;
        }
        break;
    default:
        LOG_CRITICAL(Render_Vulkan, "Vulkan failed to present swapchain due to {}!",
                     vk::to_string(result));
        UNREACHABLE();
    }

    ASSERT(fences[image_index] == nullptr);
    fences[image_index] = &fence;
    frame_index = (frame_index + 1) % static_cast<u32>(image_count);
    return recreated;
}

bool VKSwapchain::HasFramebufferChanged(const Layout::FramebufferLayout& framebuffer) const {
    // TODO(Rodrigo): Handle framebuffer pixel format changes
    return framebuffer.width != current_width || framebuffer.height != current_height;
}

void VKSwapchain::CreateSwapchain(const vk::SurfaceCapabilitiesKHR& capabilities, u32 width,
                                  u32 height, bool srgb) {
    const auto& dld{device.GetDispatchLoader()};
    const auto physical_device{device.GetPhysical()};
    const auto formats{physical_device.getSurfaceFormatsKHR(surface, dld)};
    const auto present_modes{physical_device.getSurfacePresentModesKHR(surface, dld)};

    const vk::SurfaceFormatKHR surface_format{ChooseSwapSurfaceFormat(formats, srgb)};
    const vk::PresentModeKHR present_mode{ChooseSwapPresentMode(present_modes)};

    u32 requested_image_count{capabilities.minImageCount + 1};
    if (capabilities.maxImageCount > 0 && requested_image_count > capabilities.maxImageCount) {
        requested_image_count = capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR swapchain_ci(
        {}, surface, requested_image_count, surface_format.format, surface_format.colorSpace, {}, 1,
        vk::ImageUsageFlagBits::eColorAttachment, {}, {}, {}, capabilities.currentTransform,
        vk::CompositeAlphaFlagBitsKHR::eOpaque, present_mode, false, {});

    const u32 graphics_family{device.GetGraphicsFamily()};
    const u32 present_family{device.GetPresentFamily()};
    const std::array<u32, 2> queue_indices{graphics_family, present_family};
    if (graphics_family != present_family) {
        swapchain_ci.imageSharingMode = vk::SharingMode::eConcurrent;
        swapchain_ci.queueFamilyIndexCount = static_cast<u32>(queue_indices.size());
        swapchain_ci.pQueueFamilyIndices = queue_indices.data();
    } else {
        swapchain_ci.imageSharingMode = vk::SharingMode::eExclusive;
    }

    // Request the size again to reduce the possibility of a TOCTOU race condition.
    const auto updated_capabilities = physical_device.getSurfaceCapabilitiesKHR(surface, dld);
    swapchain_ci.imageExtent = ChooseSwapExtent(updated_capabilities, width, height);
    // Don't add code within this and the swapchain creation.
    const auto dev{device.GetLogical()};
    swapchain = dev.createSwapchainKHRUnique(swapchain_ci, nullptr, dld);

    extent = swapchain_ci.imageExtent;
    current_width = extent.width;
    current_height = extent.height;
    current_srgb = srgb;

    images = dev.getSwapchainImagesKHR(*swapchain, dld);
    image_count = static_cast<u32>(images.size());
    image_format = surface_format.format;
}

void VKSwapchain::CreateSemaphores() {
    const auto dev{device.GetLogical()};
    const auto& dld{device.GetDispatchLoader()};

    present_semaphores.resize(image_count);
    for (std::size_t i = 0; i < image_count; i++) {
        present_semaphores[i] = dev.createSemaphoreUnique({}, nullptr, dld);
    }
}

void VKSwapchain::CreateImageViews() {
    const auto dev{device.GetLogical()};
    const auto& dld{device.GetDispatchLoader()};

    image_views.resize(image_count);
    for (std::size_t i = 0; i < image_count; i++) {
        const vk::ImageViewCreateInfo image_view_ci({}, images[i], vk::ImageViewType::e2D,
                                                    image_format, {},
                                                    {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
        image_views[i] = dev.createImageViewUnique(image_view_ci, nullptr, dld);
    }
}

void VKSwapchain::Destroy() {
    frame_index = 0;
    present_semaphores.clear();
    framebuffers.clear();
    image_views.clear();
    swapchain.reset();
}

} // namespace Vulkan
