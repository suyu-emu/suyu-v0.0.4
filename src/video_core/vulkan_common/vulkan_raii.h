// SPDX-FileCopyrightText: Copyright 2025 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <utility>
#include <functional>
#include <string>

#include "common/logging/log.h"

#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

/**
 * RAII wrapper for Vulkan resources.
 * Automatically manages the lifetime of Vulkan objects using RAII principles.
 */
template <typename T, typename Owner = void*, typename Dispatch = vk::InstanceDispatch>
class VulkanRaii {
public:
    using DeleterFunc = std::function<void(T, const Dispatch&)>;

    // Default constructor - creates a null handle
    VulkanRaii() : handle{}, deleter{}, dispatch{} {}

    // Constructor with handle and deleter
    VulkanRaii(T handle_, DeleterFunc deleter_, const Dispatch& dispatch_, const char* resource_name = "Vulkan resource")
        : handle{handle_}, deleter{std::move(deleter_)}, dispatch{dispatch_} {
        LOG_WARNING(Render_Vulkan, "RAII wrapper created for {}", resource_name);
    }

    // Move constructor
    VulkanRaii(VulkanRaii&& other) noexcept
        : handle{std::exchange(other.handle, VK_NULL_HANDLE)},
          deleter{std::move(other.deleter)},
          dispatch{other.dispatch} {
    }

    // Move assignment
    VulkanRaii& operator=(VulkanRaii&& other) noexcept {
        if (this != &other) {
            cleanup();
            handle = std::exchange(other.handle, VK_NULL_HANDLE);
            deleter = std::move(other.deleter);
            dispatch = other.dispatch;
        }
        return *this;
    }

    // Destructor - automatically cleans up the resource
    ~VulkanRaii() {
        if (handle != VK_NULL_HANDLE) {
            LOG_WARNING(Render_Vulkan, "RAII wrapper destroying resource");
        }
        cleanup();
    }

    // Disallow copying
    VulkanRaii(const VulkanRaii&) = delete;
    VulkanRaii& operator=(const VulkanRaii&) = delete;

    // Get the underlying handle
    T get() const noexcept {
        return handle;
    }

    // Check if the handle is valid
    bool valid() const noexcept {
        return handle != VK_NULL_HANDLE;
    }

    // Release ownership of the handle without destroying it
    T release() noexcept {
        T result = handle;
        handle = VK_NULL_HANDLE;
        return result;
    }

    // Reset the handle (destroying the current one if it exists)
    void reset(T new_handle = VK_NULL_HANDLE, DeleterFunc new_deleter = {}) {
        cleanup();
        handle = new_handle;
        deleter = std::move(new_deleter);
    }

    // Implicit conversion to handle type
    operator T() const noexcept {
        return handle;
    }

    // Dereference operator for pointer-like access
    T operator->() const noexcept {
        return handle;
    }

private:
    // Optimized cleanup function that avoids unnecessary checks in release builds
    void cleanup() noexcept {
        if (handle != VK_NULL_HANDLE) {
            if (deleter) {
                deleter(handle, dispatch);
            }
            handle = VK_NULL_HANDLE;
        }
    }

    T handle;
    DeleterFunc deleter;
    Dispatch dispatch;
};

// Common type aliases for Vulkan RAII wrappers
using RaiiInstance = VulkanRaii<VkInstance, void*, vk::InstanceDispatch>;
using RaiiDevice = VulkanRaii<VkDevice, void*, vk::DeviceDispatch>;
using RaiiSurface = VulkanRaii<VkSurfaceKHR, VkInstance, vk::InstanceDispatch>;
using RaiiSwapchain = VulkanRaii<VkSwapchainKHR, VkDevice, vk::DeviceDispatch>;
using RaiiCommandPool = VulkanRaii<VkCommandPool, VkDevice, vk::DeviceDispatch>;
using RaiiBuffer = VulkanRaii<VkBuffer, VkDevice, vk::DeviceDispatch>;
using RaiiImage = VulkanRaii<VkImage, VkDevice, vk::DeviceDispatch>;
using RaiiImageView = VulkanRaii<VkImageView, VkDevice, vk::DeviceDispatch>;
using RaiiSampler = VulkanRaii<VkSampler, VkDevice, vk::DeviceDispatch>;
using RaiiShaderModule = VulkanRaii<VkShaderModule, VkDevice, vk::DeviceDispatch>;
using RaiiPipeline = VulkanRaii<VkPipeline, VkDevice, vk::DeviceDispatch>;
using RaiiPipelineLayout = VulkanRaii<VkPipelineLayout, VkDevice, vk::DeviceDispatch>;
using RaiiDescriptorSetLayout = VulkanRaii<VkDescriptorSetLayout, VkDevice, vk::DeviceDispatch>;
using RaiiDescriptorPool = VulkanRaii<VkDescriptorPool, VkDevice, vk::DeviceDispatch>;
using RaiiSemaphore = VulkanRaii<VkSemaphore, VkDevice, vk::DeviceDispatch>;
using RaiiFence = VulkanRaii<VkFence, VkDevice, vk::DeviceDispatch>;
using RaiiDebugUtilsMessenger = VulkanRaii<VkDebugUtilsMessengerEXT, VkInstance, vk::InstanceDispatch>;

// Helper functions to create RAII wrappers

/**
 * Creates an RAII wrapper for a Vulkan instance
 */
inline RaiiInstance MakeInstance(const vk::Instance& instance, const vk::InstanceDispatch& dispatch) {
    auto deleter = [](VkInstance handle, const vk::InstanceDispatch& dld) {
        dld.vkDestroyInstance(handle, nullptr);
    };
    return RaiiInstance(*instance, deleter, dispatch, "VkInstance");
}

/**
 * Creates an RAII wrapper for a Vulkan device
 */
inline RaiiDevice MakeDevice(const vk::Device& device, const vk::DeviceDispatch& dispatch) {
    auto deleter = [](VkDevice handle, const vk::DeviceDispatch& dld) {
        dld.vkDestroyDevice(handle, nullptr);
    };
    return RaiiDevice(*device, deleter, dispatch, "VkDevice");
}

/**
 * Creates an RAII wrapper for a Vulkan surface
 */
inline RaiiSurface MakeSurface(const vk::SurfaceKHR& surface, const vk::Instance& instance, const vk::InstanceDispatch& dispatch) {
    auto deleter = [instance_ptr = *instance](VkSurfaceKHR handle, const vk::InstanceDispatch& dld) {
        dld.vkDestroySurfaceKHR(instance_ptr, handle, nullptr);
    };
    return RaiiSurface(*surface, deleter, dispatch, "VkSurfaceKHR");
}

/**
 * Creates an RAII wrapper for a Vulkan debug messenger
 */
inline RaiiDebugUtilsMessenger MakeDebugUtilsMessenger(const vk::DebugUtilsMessenger& messenger,
                                                     const vk::Instance& instance,
                                                     const vk::InstanceDispatch& dispatch) {
    auto deleter = [instance_ptr = *instance](VkDebugUtilsMessengerEXT handle, const vk::InstanceDispatch& dld) {
        dld.vkDestroyDebugUtilsMessengerEXT(instance_ptr, handle, nullptr);
    };
    return RaiiDebugUtilsMessenger(*messenger, deleter, dispatch, "VkDebugUtilsMessengerEXT");
}

/**
 * Creates an RAII wrapper for a Vulkan swapchain
 */
inline RaiiSwapchain MakeSwapchain(VkSwapchainKHR swapchain_handle, VkDevice device_handle, const vk::DeviceDispatch& dispatch) {
    auto deleter = [device_handle](VkSwapchainKHR handle, const vk::DeviceDispatch& dld) {
        dld.vkDestroySwapchainKHR(device_handle, handle, nullptr);
    };
    return RaiiSwapchain(swapchain_handle, deleter, dispatch);
}

/**
 * Creates an RAII wrapper for a Vulkan buffer
 */
inline RaiiBuffer MakeBuffer(VkBuffer buffer_handle, VkDevice device_handle, const vk::DeviceDispatch& dispatch) {
    auto deleter = [device_handle](VkBuffer handle, const vk::DeviceDispatch& dld) {
        dld.vkDestroyBuffer(device_handle, handle, nullptr);
    };
    return RaiiBuffer(buffer_handle, deleter, dispatch);
}

/**
 * Creates an RAII wrapper for a Vulkan image
 */
inline RaiiImage MakeImage(VkImage image_handle, VkDevice device_handle, const vk::DeviceDispatch& dispatch) {
    auto deleter = [device_handle](VkImage handle, const vk::DeviceDispatch& dld) {
        dld.vkDestroyImage(device_handle, handle, nullptr);
    };
    return RaiiImage(image_handle, deleter, dispatch);
}

/**
 * Creates an RAII wrapper for a Vulkan image view
 */
inline RaiiImageView MakeImageView(VkImageView view_handle, VkDevice device_handle, const vk::DeviceDispatch& dispatch) {
    auto deleter = [device_handle](VkImageView handle, const vk::DeviceDispatch& dld) {
        dld.vkDestroyImageView(device_handle, handle, nullptr);
    };
    return RaiiImageView(view_handle, deleter, dispatch);
}

/**
 * Creates an RAII wrapper for a Vulkan semaphore
 */
inline RaiiSemaphore MakeSemaphore(VkSemaphore semaphore_handle, VkDevice device_handle, const vk::DeviceDispatch& dispatch) {
    auto deleter = [device_handle](VkSemaphore handle, const vk::DeviceDispatch& dld) {
        dld.vkDestroySemaphore(device_handle, handle, nullptr);
    };
    return RaiiSemaphore(semaphore_handle, deleter, dispatch);
}

/**
 * Creates an RAII wrapper for a Vulkan fence
 */
inline RaiiFence MakeFence(VkFence fence_handle, VkDevice device_handle, const vk::DeviceDispatch& dispatch) {
    auto deleter = [device_handle](VkFence handle, const vk::DeviceDispatch& dld) {
        dld.vkDestroyFence(device_handle, handle, nullptr);
    };
    return RaiiFence(fence_handle, deleter, dispatch);
}

} // namespace Vulkan
