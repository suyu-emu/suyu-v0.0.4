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
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_resource_manager.h"
#include "video_core/renderer_vulkan/vk_swapchain.h"
#include "video_core/renderer_vulkan/wrapper.h"

namespace Vulkan {

namespace {

VkSurfaceFormatKHR ChooseSwapSurfaceFormat(vk::Span<VkSurfaceFormatKHR> formats, bool srgb) {
    if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
        VkSurfaceFormatKHR format;
        format.format = VK_FORMAT_B8G8R8A8_UNORM;
        format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        return format;
    }
    const auto& found = std::find_if(formats.begin(), formats.end(), [srgb](const auto& format) {
        const auto request_format = srgb ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_B8G8R8A8_UNORM;
        return format.format == request_format &&
               format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    });
    return found != formats.end() ? *found : formats[0];
}

VkPresentModeKHR ChooseSwapPresentMode(vk::Span<VkPresentModeKHR> modes) {
    // Mailbox doesn't lock the application like fifo (vsync), prefer it
    const auto found = std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_MAILBOX_KHR);
    return found != modes.end() ? *found : VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, u32 width, u32 height) {
    constexpr auto undefined_size{std::numeric_limits<u32>::max()};
    if (capabilities.currentExtent.width != undefined_size) {
        return capabilities.currentExtent;
    }
    VkExtent2D extent;
    extent.width = std::max(capabilities.minImageExtent.width,
                            std::min(capabilities.maxImageExtent.width, width));
    extent.height = std::max(capabilities.minImageExtent.height,
                             std::min(capabilities.maxImageExtent.height, height));
    return extent;
}

} // Anonymous namespace

VKSwapchain::VKSwapchain(VkSurfaceKHR surface, const VKDevice& device)
    : surface{surface}, device{device} {}

VKSwapchain::~VKSwapchain() = default;

void VKSwapchain::Create(u32 width, u32 height, bool srgb) {
    const auto physical_device = device.GetPhysical();
    const auto capabilities{physical_device.GetSurfaceCapabilitiesKHR(surface)};
    if (capabilities.maxImageExtent.width == 0 || capabilities.maxImageExtent.height == 0) {
        return;
    }

    device.GetLogical().WaitIdle();
    Destroy();

    CreateSwapchain(capabilities, width, height, srgb);
    CreateSemaphores();
    CreateImageViews();

    fences.resize(image_count, nullptr);
}

void VKSwapchain::AcquireNextImage() {
    device.GetLogical().AcquireNextImageKHR(*swapchain, std::numeric_limits<u64>::max(),
                                            *present_semaphores[frame_index], {}, &image_index);

    if (auto& fence = fences[image_index]; fence) {
        fence->Wait();
        fence->Release();
        fence = nullptr;
    }
}

bool VKSwapchain::Present(VkSemaphore render_semaphore, VKFence& fence) {
    const VkSemaphore present_semaphore{*present_semaphores[frame_index]};
    const std::array<VkSemaphore, 2> semaphores{present_semaphore, render_semaphore};
    const auto present_queue{device.GetPresentQueue()};
    bool recreated = false;

    VkPresentInfoKHR present_info;
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pNext = nullptr;
    present_info.waitSemaphoreCount = render_semaphore ? 2U : 1U;
    present_info.pWaitSemaphores = semaphores.data();
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapchain.address();
    present_info.pImageIndices = &image_index;
    present_info.pResults = nullptr;

    switch (const VkResult result = present_queue.Present(present_info)) {
    case VK_SUCCESS:
        break;
    case VK_SUBOPTIMAL_KHR:
        LOG_DEBUG(Render_Vulkan, "Suboptimal swapchain");
        break;
    case VK_ERROR_OUT_OF_DATE_KHR:
        if (current_width > 0 && current_height > 0) {
            Create(current_width, current_height, current_srgb);
            recreated = true;
        }
        break;
    default:
        LOG_CRITICAL(Render_Vulkan, "Failed to present with error {}", vk::ToString(result));
        break;
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

void VKSwapchain::CreateSwapchain(const VkSurfaceCapabilitiesKHR& capabilities, u32 width,
                                  u32 height, bool srgb) {
    const auto physical_device{device.GetPhysical()};
    const auto formats{physical_device.GetSurfaceFormatsKHR(surface)};
    const auto present_modes{physical_device.GetSurfacePresentModesKHR(surface)};

    const VkSurfaceFormatKHR surface_format{ChooseSwapSurfaceFormat(formats, srgb)};
    const VkPresentModeKHR present_mode{ChooseSwapPresentMode(present_modes)};

    u32 requested_image_count{capabilities.minImageCount + 1};
    if (capabilities.maxImageCount > 0 && requested_image_count > capabilities.maxImageCount) {
        requested_image_count = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchain_ci;
    swapchain_ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_ci.pNext = nullptr;
    swapchain_ci.flags = 0;
    swapchain_ci.surface = surface;
    swapchain_ci.minImageCount = requested_image_count;
    swapchain_ci.imageFormat = surface_format.format;
    swapchain_ci.imageColorSpace = surface_format.colorSpace;
    swapchain_ci.imageArrayLayers = 1;
    swapchain_ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_ci.queueFamilyIndexCount = 0;
    swapchain_ci.pQueueFamilyIndices = nullptr;
    swapchain_ci.preTransform = capabilities.currentTransform;
    swapchain_ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_ci.presentMode = present_mode;
    swapchain_ci.clipped = VK_FALSE;
    swapchain_ci.oldSwapchain = nullptr;

    const u32 graphics_family{device.GetGraphicsFamily()};
    const u32 present_family{device.GetPresentFamily()};
    const std::array<u32, 2> queue_indices{graphics_family, present_family};
    if (graphics_family != present_family) {
        swapchain_ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_ci.queueFamilyIndexCount = static_cast<u32>(queue_indices.size());
        swapchain_ci.pQueueFamilyIndices = queue_indices.data();
    } else {
        swapchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    // Request the size again to reduce the possibility of a TOCTOU race condition.
    const auto updated_capabilities = physical_device.GetSurfaceCapabilitiesKHR(surface);
    swapchain_ci.imageExtent = ChooseSwapExtent(updated_capabilities, width, height);
    // Don't add code within this and the swapchain creation.
    swapchain = device.GetLogical().CreateSwapchainKHR(swapchain_ci);

    extent = swapchain_ci.imageExtent;
    current_width = extent.width;
    current_height = extent.height;
    current_srgb = srgb;

    images = swapchain.GetImages();
    image_count = static_cast<u32>(images.size());
    image_format = surface_format.format;
}

void VKSwapchain::CreateSemaphores() {
    present_semaphores.resize(image_count);
    std::generate(present_semaphores.begin(), present_semaphores.end(),
                  [this] { return device.GetLogical().CreateSemaphore(); });
}

void VKSwapchain::CreateImageViews() {
    VkImageViewCreateInfo ci;
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ci.pNext = nullptr;
    ci.flags = 0;
    // ci.image
    ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ci.format = image_format;
    ci.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                     VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ci.subresourceRange.baseMipLevel = 0;
    ci.subresourceRange.levelCount = 1;
    ci.subresourceRange.baseArrayLayer = 0;
    ci.subresourceRange.layerCount = 1;

    image_views.resize(image_count);
    for (std::size_t i = 0; i < image_count; i++) {
        ci.image = images[i];
        image_views[i] = device.GetLogical().CreateImageView(ci);
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
