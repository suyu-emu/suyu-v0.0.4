// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <exception>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include "common/common_types.h"

namespace Vulkan::vk {

/**
 * Span for Vulkan arrays.
 * Based on std::span but optimized for array access instead of iterators.
 * Size returns uint32_t instead of size_t to ease interaction with Vulkan functions.
 */
template <typename T>
class Span {
public:
    using value_type = T;
    using size_type = u32;
    using difference_type = std::ptrdiff_t;
    using reference = const T&;
    using const_reference = const T&;
    using pointer = const T*;
    using const_pointer = const T*;
    using iterator = const T*;
    using const_iterator = const T*;

    /// Construct an empty span.
    constexpr Span() noexcept = default;

    /// Construct a span from a single element.
    constexpr Span(const T& value) noexcept : ptr{&value}, num{1} {}

    /// Construct a span from a range.
    template <typename Range>
    // requires std::data(const Range&)
    // requires std::size(const Range&)
    constexpr Span(const Range& range) : ptr{std::data(range)}, num{std::size(range)} {}

    /// Construct a span from a pointer and a size.
    /// This is inteded for subranges.
    constexpr Span(const T* ptr, std::size_t num) noexcept : ptr{ptr}, num{num} {}

    /// Returns the data pointer by the span.
    constexpr const T* data() const noexcept {
        return ptr;
    }

    /// Returns the number of elements in the span.
    /// @note Returns a 32 bits integer because most Vulkan functions expect this type.
    constexpr u32 size() const noexcept {
        return static_cast<u32>(num);
    }

    /// Returns true when the span is empty.
    constexpr bool empty() const noexcept {
        return num == 0;
    }

    /// Returns a reference to the element in the passed index.
    /// @pre: index < size()
    constexpr const T& operator[](std::size_t index) const noexcept {
        return ptr[index];
    }

    /// Returns an iterator to the beginning of the span.
    constexpr const T* begin() const noexcept {
        return ptr;
    }

    /// Returns an iterator to the end of the span.
    constexpr const T* end() const noexcept {
        return ptr + num;
    }

    /// Returns an iterator to the beginning of the span.
    constexpr const T* cbegin() const noexcept {
        return ptr;
    }

    /// Returns an iterator to the end of the span.
    constexpr const T* cend() const noexcept {
        return ptr + num;
    }

private:
    const T* ptr = nullptr;
    std::size_t num = 0;
};

/// Vulkan exception generated from a VkResult.
class Exception final : public std::exception {
public:
    /// Construct the exception with a result.
    /// @pre result != VK_SUCCESS
    explicit Exception(VkResult result_) : result{result_} {}
    virtual ~Exception() = default;

    const char* what() const noexcept override;

private:
    VkResult result;
};

/// Converts a VkResult enum into a rodata string
const char* ToString(VkResult) noexcept;

/// Throws a Vulkan exception if result is not success.
inline void Check(VkResult result) {
    if (result != VK_SUCCESS) {
        throw Exception(result);
    }
}

/// Throws a Vulkan exception if result is an error.
/// @return result
inline VkResult Filter(VkResult result) {
    if (result < 0) {
        throw Exception(result);
    }
    return result;
}

/// Table holding Vulkan instance function pointers.
struct InstanceDispatch {
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;

    PFN_vkCreateInstance vkCreateInstance;
    PFN_vkDestroyInstance vkDestroyInstance;
    PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties;

    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
    PFN_vkCreateDevice vkCreateDevice;
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
    PFN_vkDestroyDevice vkDestroyDevice;
    PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR;
    PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties;
    PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
    PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
    PFN_vkGetPhysicalDeviceFeatures2KHR vkGetPhysicalDeviceFeatures2KHR;
    PFN_vkGetPhysicalDeviceFormatProperties vkGetPhysicalDeviceFormatProperties;
    PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
    PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
    PFN_vkGetPhysicalDeviceProperties2KHR vkGetPhysicalDeviceProperties2KHR;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
    PFN_vkQueuePresentKHR vkQueuePresentKHR;
};

/// Table holding Vulkan device function pointers.
struct DeviceDispatch : public InstanceDispatch {
    PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
    PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
    PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets;
    PFN_vkAllocateMemory vkAllocateMemory;
    PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
    PFN_vkBindBufferMemory vkBindBufferMemory;
    PFN_vkBindImageMemory vkBindImageMemory;
    PFN_vkCmdBeginQuery vkCmdBeginQuery;
    PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass;
    PFN_vkCmdBeginTransformFeedbackEXT vkCmdBeginTransformFeedbackEXT;
    PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets;
    PFN_vkCmdBindIndexBuffer vkCmdBindIndexBuffer;
    PFN_vkCmdBindPipeline vkCmdBindPipeline;
    PFN_vkCmdBindTransformFeedbackBuffersEXT vkCmdBindTransformFeedbackBuffersEXT;
    PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers;
    PFN_vkCmdBlitImage vkCmdBlitImage;
    PFN_vkCmdClearAttachments vkCmdClearAttachments;
    PFN_vkCmdCopyBuffer vkCmdCopyBuffer;
    PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage;
    PFN_vkCmdCopyImage vkCmdCopyImage;
    PFN_vkCmdCopyImageToBuffer vkCmdCopyImageToBuffer;
    PFN_vkCmdDispatch vkCmdDispatch;
    PFN_vkCmdDraw vkCmdDraw;
    PFN_vkCmdDrawIndexed vkCmdDrawIndexed;
    PFN_vkCmdEndQuery vkCmdEndQuery;
    PFN_vkCmdEndRenderPass vkCmdEndRenderPass;
    PFN_vkCmdEndTransformFeedbackEXT vkCmdEndTransformFeedbackEXT;
    PFN_vkCmdFillBuffer vkCmdFillBuffer;
    PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
    PFN_vkCmdPushConstants vkCmdPushConstants;
    PFN_vkCmdSetBlendConstants vkCmdSetBlendConstants;
    PFN_vkCmdSetCheckpointNV vkCmdSetCheckpointNV;
    PFN_vkCmdSetDepthBias vkCmdSetDepthBias;
    PFN_vkCmdSetDepthBounds vkCmdSetDepthBounds;
    PFN_vkCmdSetScissor vkCmdSetScissor;
    PFN_vkCmdSetStencilCompareMask vkCmdSetStencilCompareMask;
    PFN_vkCmdSetStencilReference vkCmdSetStencilReference;
    PFN_vkCmdSetStencilWriteMask vkCmdSetStencilWriteMask;
    PFN_vkCmdSetViewport vkCmdSetViewport;
    PFN_vkCreateBuffer vkCreateBuffer;
    PFN_vkCreateBufferView vkCreateBufferView;
    PFN_vkCreateCommandPool vkCreateCommandPool;
    PFN_vkCreateComputePipelines vkCreateComputePipelines;
    PFN_vkCreateDescriptorPool vkCreateDescriptorPool;
    PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout;
    PFN_vkCreateDescriptorUpdateTemplateKHR vkCreateDescriptorUpdateTemplateKHR;
    PFN_vkCreateFence vkCreateFence;
    PFN_vkCreateFramebuffer vkCreateFramebuffer;
    PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines;
    PFN_vkCreateImage vkCreateImage;
    PFN_vkCreateImageView vkCreateImageView;
    PFN_vkCreatePipelineLayout vkCreatePipelineLayout;
    PFN_vkCreateQueryPool vkCreateQueryPool;
    PFN_vkCreateRenderPass vkCreateRenderPass;
    PFN_vkCreateSampler vkCreateSampler;
    PFN_vkCreateSemaphore vkCreateSemaphore;
    PFN_vkCreateShaderModule vkCreateShaderModule;
    PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
    PFN_vkDestroyBuffer vkDestroyBuffer;
    PFN_vkDestroyBufferView vkDestroyBufferView;
    PFN_vkDestroyCommandPool vkDestroyCommandPool;
    PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool;
    PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout;
    PFN_vkDestroyDescriptorUpdateTemplateKHR vkDestroyDescriptorUpdateTemplateKHR;
    PFN_vkDestroyFence vkDestroyFence;
    PFN_vkDestroyFramebuffer vkDestroyFramebuffer;
    PFN_vkDestroyImage vkDestroyImage;
    PFN_vkDestroyImageView vkDestroyImageView;
    PFN_vkDestroyPipeline vkDestroyPipeline;
    PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout;
    PFN_vkDestroyQueryPool vkDestroyQueryPool;
    PFN_vkDestroyRenderPass vkDestroyRenderPass;
    PFN_vkDestroySampler vkDestroySampler;
    PFN_vkDestroySemaphore vkDestroySemaphore;
    PFN_vkDestroyShaderModule vkDestroyShaderModule;
    PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
    PFN_vkDeviceWaitIdle vkDeviceWaitIdle;
    PFN_vkEndCommandBuffer vkEndCommandBuffer;
    PFN_vkFreeCommandBuffers vkFreeCommandBuffers;
    PFN_vkFreeDescriptorSets vkFreeDescriptorSets;
    PFN_vkFreeMemory vkFreeMemory;
    PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
    PFN_vkGetDeviceQueue vkGetDeviceQueue;
    PFN_vkGetFenceStatus vkGetFenceStatus;
    PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements;
    PFN_vkGetQueryPoolResults vkGetQueryPoolResults;
    PFN_vkGetQueueCheckpointDataNV vkGetQueueCheckpointDataNV;
    PFN_vkMapMemory vkMapMemory;
    PFN_vkQueueSubmit vkQueueSubmit;
    PFN_vkResetFences vkResetFences;
    PFN_vkResetQueryPoolEXT vkResetQueryPoolEXT;
    PFN_vkUnmapMemory vkUnmapMemory;
    PFN_vkUpdateDescriptorSetWithTemplateKHR vkUpdateDescriptorSetWithTemplateKHR;
    PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets;
    PFN_vkWaitForFences vkWaitForFences;
};

/// Loads instance agnostic function pointers.
/// @return True on success, false on error.
bool Load(InstanceDispatch&) noexcept;

/// Loads instance function pointers.
/// @return True on success, false on error.
bool Load(VkInstance, InstanceDispatch&) noexcept;

void Destroy(VkInstance, const InstanceDispatch&) noexcept;
void Destroy(VkDevice, const InstanceDispatch&) noexcept;

void Destroy(VkDevice, VkBuffer, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkBufferView, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkCommandPool, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkDescriptorPool, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkDescriptorSetLayout, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkDescriptorUpdateTemplateKHR, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkDeviceMemory, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkFence, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkFramebuffer, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkImage, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkImageView, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkPipeline, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkPipelineLayout, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkQueryPool, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkRenderPass, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkSampler, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkSwapchainKHR, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkSemaphore, const DeviceDispatch&) noexcept;
void Destroy(VkDevice, VkShaderModule, const DeviceDispatch&) noexcept;
void Destroy(VkInstance, VkDebugUtilsMessengerEXT, const InstanceDispatch&) noexcept;
void Destroy(VkInstance, VkSurfaceKHR, const InstanceDispatch&) noexcept;

VkResult Free(VkDevice, VkDescriptorPool, Span<VkDescriptorSet>, const DeviceDispatch&) noexcept;
VkResult Free(VkDevice, VkCommandPool, Span<VkCommandBuffer>, const DeviceDispatch&) noexcept;

template <typename Type, typename OwnerType, typename Dispatch>
class Handle;

/// Handle with an owning type.
/// Analogue to std::unique_ptr.
template <typename Type, typename OwnerType, typename Dispatch>
class Handle {
public:
    /// Construct a handle and hold it's ownership.
    explicit Handle(Type handle_, OwnerType owner_, const Dispatch& dld_) noexcept
        : handle{handle_}, owner{owner_}, dld{&dld_} {}

    /// Construct an empty handle.
    Handle() = default;

    /// Copying Vulkan objects is not supported and will never be.
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    /// Construct a handle transfering the ownership from another handle.
    Handle(Handle&& rhs) noexcept
        : handle{std::exchange(rhs.handle, nullptr)}, owner{rhs.owner}, dld{rhs.dld} {}

    /// Assign the current handle transfering the ownership from another handle.
    /// Destroys any previously held object.
    Handle& operator=(Handle&& rhs) noexcept {
        Release();
        handle = std::exchange(rhs.handle, nullptr);
        owner = rhs.owner;
        dld = rhs.dld;
        return *this;
    }

    /// Destroys the current handle if it existed.
    ~Handle() noexcept {
        Release();
    }

    /// Destroys any held object.
    void reset() noexcept {
        Release();
        handle = nullptr;
    }

    /// Returns the address of the held object.
    /// Intended for Vulkan structures that expect a pointer to an array.
    const Type* address() const noexcept {
        return std::addressof(handle);
    }

    /// Returns the held Vulkan handle.
    Type operator*() const noexcept {
        return handle;
    }

    /// Returns true when there's a held object.
    explicit operator bool() const noexcept {
        return handle != nullptr;
    }

protected:
    Type handle = nullptr;
    OwnerType owner = nullptr;
    const Dispatch* dld = nullptr;

private:
    /// Destroys the held object if it exists.
    void Release() noexcept {
        if (handle) {
            Destroy(owner, handle, *dld);
        }
    }
};

/// Dummy type used to specify a handle has no owner.
struct NoOwner {};

/// Handle without an owning type.
/// Analogue to std::unique_ptr
template <typename Type, typename Dispatch>
class Handle<Type, NoOwner, Dispatch> {
public:
    /// Construct a handle and hold it's ownership.
    explicit Handle(Type handle_, const Dispatch& dld_) noexcept : handle{handle_}, dld{&dld_} {}

    /// Construct an empty handle.
    Handle() noexcept = default;

    /// Copying Vulkan objects is not supported and will never be.
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    /// Construct a handle transfering ownership from another handle.
    Handle(Handle&& rhs) noexcept : handle{std::exchange(rhs.handle, nullptr)}, dld{rhs.dld} {}

    /// Assign the current handle transfering the ownership from another handle.
    /// Destroys any previously held object.
    Handle& operator=(Handle&& rhs) noexcept {
        Release();
        handle = std::exchange(rhs.handle, nullptr);
        dld = rhs.dld;
        return *this;
    }

    /// Destroys the current handle if it existed.
    ~Handle() noexcept {
        Release();
    }

    /// Destroys any held object.
    void reset() noexcept {
        Release();
        handle = nullptr;
    }

    /// Returns the address of the held object.
    /// Intended for Vulkan structures that expect a pointer to an array.
    const Type* address() const noexcept {
        return std::addressof(handle);
    }

    /// Returns the held Vulkan handle.
    Type operator*() const noexcept {
        return handle;
    }

    /// Returns true when there's a held object.
    operator bool() const noexcept {
        return handle != nullptr;
    }

protected:
    Type handle = nullptr;
    const Dispatch* dld = nullptr;

private:
    /// Destroys the held object if it exists.
    void Release() noexcept {
        if (handle) {
            Destroy(handle, *dld);
        }
    }
};

/// Array of a pool allocation.
/// Analogue to std::vector
template <typename AllocationType, typename PoolType>
class PoolAllocations {
public:
    /// Construct an empty allocation.
    PoolAllocations() = default;

    /// Construct an allocation. Errors are reported through IsOutOfPoolMemory().
    explicit PoolAllocations(std::unique_ptr<AllocationType[]> allocations, std::size_t num,
                             VkDevice device, PoolType pool, const DeviceDispatch& dld) noexcept
        : allocations{std::move(allocations)}, num{num}, device{device}, pool{pool}, dld{&dld} {}

    /// Copying Vulkan allocations is not supported and will never be.
    PoolAllocations(const PoolAllocations&) = delete;
    PoolAllocations& operator=(const PoolAllocations&) = delete;

    /// Construct an allocation transfering ownership from another allocation.
    PoolAllocations(PoolAllocations&& rhs) noexcept
        : allocations{std::move(rhs.allocations)}, num{rhs.num}, device{rhs.device}, pool{rhs.pool},
          dld{rhs.dld} {}

    /// Assign an allocation transfering ownership from another allocation.
    /// Releases any previously held allocation.
    PoolAllocations& operator=(PoolAllocations&& rhs) noexcept {
        Release();
        allocations = std::move(rhs.allocations);
        num = rhs.num;
        device = rhs.device;
        pool = rhs.pool;
        dld = rhs.dld;
        return *this;
    }

    /// Destroys any held allocation.
    ~PoolAllocations() {
        Release();
    }

    /// Returns the number of allocations.
    std::size_t size() const noexcept {
        return num;
    }

    /// Returns a pointer to the array of allocations.
    AllocationType const* data() const noexcept {
        return allocations.get();
    }

    /// Returns the allocation in the specified index.
    /// @pre index < size()
    AllocationType operator[](std::size_t index) const noexcept {
        return allocations[index];
    }

    /// True when a pool fails to construct.
    bool IsOutOfPoolMemory() const noexcept {
        return !device;
    }

private:
    /// Destroys the held allocations if they exist.
    void Release() noexcept {
        if (!allocations) {
            return;
        }
        const Span<AllocationType> span(allocations.get(), num);
        const VkResult result = Free(device, pool, span, *dld);
        // There's no way to report errors from a destructor.
        if (result != VK_SUCCESS) {
            std::terminate();
        }
    }

    std::unique_ptr<AllocationType[]> allocations;
    std::size_t num = 0;
    VkDevice device = nullptr;
    PoolType pool = nullptr;
    const DeviceDispatch* dld = nullptr;
};

using BufferView = Handle<VkBufferView, VkDevice, DeviceDispatch>;
using DebugCallback = Handle<VkDebugUtilsMessengerEXT, VkInstance, InstanceDispatch>;
using DescriptorSetLayout = Handle<VkDescriptorSetLayout, VkDevice, DeviceDispatch>;
using DescriptorUpdateTemplateKHR = Handle<VkDescriptorUpdateTemplateKHR, VkDevice, DeviceDispatch>;
using Framebuffer = Handle<VkFramebuffer, VkDevice, DeviceDispatch>;
using ImageView = Handle<VkImageView, VkDevice, DeviceDispatch>;
using Pipeline = Handle<VkPipeline, VkDevice, DeviceDispatch>;
using PipelineLayout = Handle<VkPipelineLayout, VkDevice, DeviceDispatch>;
using QueryPool = Handle<VkQueryPool, VkDevice, DeviceDispatch>;
using RenderPass = Handle<VkRenderPass, VkDevice, DeviceDispatch>;
using Sampler = Handle<VkSampler, VkDevice, DeviceDispatch>;
using Semaphore = Handle<VkSemaphore, VkDevice, DeviceDispatch>;
using ShaderModule = Handle<VkShaderModule, VkDevice, DeviceDispatch>;
using SurfaceKHR = Handle<VkSurfaceKHR, VkInstance, InstanceDispatch>;

using DescriptorSets = PoolAllocations<VkDescriptorSet, VkDescriptorPool>;
using CommandBuffers = PoolAllocations<VkCommandBuffer, VkCommandPool>;

} // namespace Vulkan::vk
