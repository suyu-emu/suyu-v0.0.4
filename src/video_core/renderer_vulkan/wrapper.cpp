// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <exception>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "common/common_types.h"

#include "video_core/renderer_vulkan/wrapper.h"

namespace Vulkan::vk {

namespace {

template <typename T>
bool Proc(T& result, const InstanceDispatch& dld, const char* proc_name,
          VkInstance instance = nullptr) noexcept {
    result = reinterpret_cast<T>(dld.vkGetInstanceProcAddr(instance, proc_name));
    return result != nullptr;
}

template <typename T>
void Proc(T& result, const DeviceDispatch& dld, const char* proc_name, VkDevice device) noexcept {
    result = reinterpret_cast<T>(dld.vkGetDeviceProcAddr(device, proc_name));
}

void Load(VkDevice device, DeviceDispatch& dld) noexcept {
#define X(name) Proc(dld.name, dld, #name, device)
    X(vkAcquireNextImageKHR);
    X(vkAllocateCommandBuffers);
    X(vkAllocateDescriptorSets);
    X(vkAllocateMemory);
    X(vkBeginCommandBuffer);
    X(vkBindBufferMemory);
    X(vkBindImageMemory);
    X(vkCmdBeginQuery);
    X(vkCmdBeginRenderPass);
    X(vkCmdBeginTransformFeedbackEXT);
    X(vkCmdBindDescriptorSets);
    X(vkCmdBindIndexBuffer);
    X(vkCmdBindPipeline);
    X(vkCmdBindTransformFeedbackBuffersEXT);
    X(vkCmdBindVertexBuffers);
    X(vkCmdBlitImage);
    X(vkCmdClearAttachments);
    X(vkCmdCopyBuffer);
    X(vkCmdCopyBufferToImage);
    X(vkCmdCopyImage);
    X(vkCmdCopyImageToBuffer);
    X(vkCmdDispatch);
    X(vkCmdDraw);
    X(vkCmdDrawIndexed);
    X(vkCmdEndQuery);
    X(vkCmdEndRenderPass);
    X(vkCmdEndTransformFeedbackEXT);
    X(vkCmdFillBuffer);
    X(vkCmdPipelineBarrier);
    X(vkCmdPushConstants);
    X(vkCmdSetBlendConstants);
    X(vkCmdSetCheckpointNV);
    X(vkCmdSetDepthBias);
    X(vkCmdSetDepthBounds);
    X(vkCmdSetScissor);
    X(vkCmdSetStencilCompareMask);
    X(vkCmdSetStencilReference);
    X(vkCmdSetStencilWriteMask);
    X(vkCmdSetViewport);
    X(vkCreateBuffer);
    X(vkCreateBufferView);
    X(vkCreateCommandPool);
    X(vkCreateComputePipelines);
    X(vkCreateDescriptorPool);
    X(vkCreateDescriptorSetLayout);
    X(vkCreateDescriptorUpdateTemplateKHR);
    X(vkCreateFence);
    X(vkCreateFramebuffer);
    X(vkCreateGraphicsPipelines);
    X(vkCreateImage);
    X(vkCreateImageView);
    X(vkCreatePipelineLayout);
    X(vkCreateQueryPool);
    X(vkCreateRenderPass);
    X(vkCreateSampler);
    X(vkCreateSemaphore);
    X(vkCreateShaderModule);
    X(vkCreateSwapchainKHR);
    X(vkDestroyBuffer);
    X(vkDestroyBufferView);
    X(vkDestroyCommandPool);
    X(vkDestroyDescriptorPool);
    X(vkDestroyDescriptorSetLayout);
    X(vkDestroyDescriptorUpdateTemplateKHR);
    X(vkDestroyFence);
    X(vkDestroyFramebuffer);
    X(vkDestroyImage);
    X(vkDestroyImageView);
    X(vkDestroyPipeline);
    X(vkDestroyPipelineLayout);
    X(vkDestroyQueryPool);
    X(vkDestroyRenderPass);
    X(vkDestroySampler);
    X(vkDestroySemaphore);
    X(vkDestroyShaderModule);
    X(vkDestroySwapchainKHR);
    X(vkDeviceWaitIdle);
    X(vkEndCommandBuffer);
    X(vkFreeCommandBuffers);
    X(vkFreeDescriptorSets);
    X(vkFreeMemory);
    X(vkGetBufferMemoryRequirements);
    X(vkGetDeviceQueue);
    X(vkGetFenceStatus);
    X(vkGetImageMemoryRequirements);
    X(vkGetQueryPoolResults);
    X(vkGetQueueCheckpointDataNV);
    X(vkMapMemory);
    X(vkQueueSubmit);
    X(vkResetFences);
    X(vkResetQueryPoolEXT);
    X(vkUnmapMemory);
    X(vkUpdateDescriptorSetWithTemplateKHR);
    X(vkUpdateDescriptorSets);
    X(vkWaitForFences);
#undef X
}

} // Anonymous namespace

bool Load(InstanceDispatch& dld) noexcept {
#define X(name) Proc(dld.name, dld, #name)
    return X(vkCreateInstance) && X(vkEnumerateInstanceExtensionProperties);
#undef X
}

bool Load(VkInstance instance, InstanceDispatch& dld) noexcept {
#define X(name) Proc(dld.name, dld, #name, instance)
    // These functions may fail to load depending on the enabled extensions.
    // Don't return a failure on these.
    X(vkCreateDebugUtilsMessengerEXT);
    X(vkDestroyDebugUtilsMessengerEXT);
    X(vkDestroySurfaceKHR);
    X(vkGetPhysicalDeviceFeatures2KHR);
    X(vkGetPhysicalDeviceProperties2KHR);
    X(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    X(vkGetPhysicalDeviceSurfaceFormatsKHR);
    X(vkGetPhysicalDeviceSurfacePresentModesKHR);
    X(vkGetPhysicalDeviceSurfaceSupportKHR);
    X(vkGetSwapchainImagesKHR);
    X(vkQueuePresentKHR);

    return X(vkCreateDevice) && X(vkDestroyDevice) && X(vkDestroyDevice) &&
           X(vkEnumerateDeviceExtensionProperties) && X(vkEnumeratePhysicalDevices) &&
           X(vkGetDeviceProcAddr) && X(vkGetPhysicalDeviceFormatProperties) &&
           X(vkGetPhysicalDeviceMemoryProperties) && X(vkGetPhysicalDeviceProperties) &&
           X(vkGetPhysicalDeviceQueueFamilyProperties);
#undef X
}

const char* Exception::what() const noexcept {
    return ToString(result);
}

const char* ToString(VkResult result) noexcept {
    switch (result) {
    case VkResult::VK_SUCCESS:
        return "VK_SUCCESS";
    case VkResult::VK_NOT_READY:
        return "VK_NOT_READY";
    case VkResult::VK_TIMEOUT:
        return "VK_TIMEOUT";
    case VkResult::VK_EVENT_SET:
        return "VK_EVENT_SET";
    case VkResult::VK_EVENT_RESET:
        return "VK_EVENT_RESET";
    case VkResult::VK_INCOMPLETE:
        return "VK_INCOMPLETE";
    case VkResult::VK_ERROR_OUT_OF_HOST_MEMORY:
        return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VkResult::VK_ERROR_INITIALIZATION_FAILED:
        return "VK_ERROR_INITIALIZATION_FAILED";
    case VkResult::VK_ERROR_DEVICE_LOST:
        return "VK_ERROR_DEVICE_LOST";
    case VkResult::VK_ERROR_MEMORY_MAP_FAILED:
        return "VK_ERROR_MEMORY_MAP_FAILED";
    case VkResult::VK_ERROR_LAYER_NOT_PRESENT:
        return "VK_ERROR_LAYER_NOT_PRESENT";
    case VkResult::VK_ERROR_EXTENSION_NOT_PRESENT:
        return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VkResult::VK_ERROR_FEATURE_NOT_PRESENT:
        return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VkResult::VK_ERROR_INCOMPATIBLE_DRIVER:
        return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VkResult::VK_ERROR_TOO_MANY_OBJECTS:
        return "VK_ERROR_TOO_MANY_OBJECTS";
    case VkResult::VK_ERROR_FORMAT_NOT_SUPPORTED:
        return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VkResult::VK_ERROR_FRAGMENTED_POOL:
        return "VK_ERROR_FRAGMENTED_POOL";
    case VkResult::VK_ERROR_OUT_OF_POOL_MEMORY:
        return "VK_ERROR_OUT_OF_POOL_MEMORY";
    case VkResult::VK_ERROR_INVALID_EXTERNAL_HANDLE:
        return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
    case VkResult::VK_ERROR_SURFACE_LOST_KHR:
        return "VK_ERROR_SURFACE_LOST_KHR";
    case VkResult::VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
        return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VkResult::VK_SUBOPTIMAL_KHR:
        return "VK_SUBOPTIMAL_KHR";
    case VkResult::VK_ERROR_OUT_OF_DATE_KHR:
        return "VK_ERROR_OUT_OF_DATE_KHR";
    case VkResult::VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
        return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
    case VkResult::VK_ERROR_VALIDATION_FAILED_EXT:
        return "VK_ERROR_VALIDATION_FAILED_EXT";
    case VkResult::VK_ERROR_INVALID_SHADER_NV:
        return "VK_ERROR_INVALID_SHADER_NV";
    case VkResult::VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
        return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
    case VkResult::VK_ERROR_FRAGMENTATION_EXT:
        return "VK_ERROR_FRAGMENTATION_EXT";
    case VkResult::VK_ERROR_NOT_PERMITTED_EXT:
        return "VK_ERROR_NOT_PERMITTED_EXT";
    case VkResult::VK_ERROR_INVALID_DEVICE_ADDRESS_EXT:
        return "VK_ERROR_INVALID_DEVICE_ADDRESS_EXT";
    case VkResult::VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
        return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
    }
    return "Unknown";
}

void Destroy(VkInstance instance, const InstanceDispatch& dld) noexcept {
    dld.vkDestroyInstance(instance, nullptr);
}

void Destroy(VkDevice device, const InstanceDispatch& dld) noexcept {
    dld.vkDestroyDevice(device, nullptr);
}

void Destroy(VkDevice device, VkBuffer handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroyBuffer(device, handle, nullptr);
}

void Destroy(VkDevice device, VkBufferView handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroyBufferView(device, handle, nullptr);
}

void Destroy(VkDevice device, VkCommandPool handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroyCommandPool(device, handle, nullptr);
}

void Destroy(VkDevice device, VkDescriptorPool handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroyDescriptorPool(device, handle, nullptr);
}

void Destroy(VkDevice device, VkDescriptorSetLayout handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroyDescriptorSetLayout(device, handle, nullptr);
}

void Destroy(VkDevice device, VkDescriptorUpdateTemplateKHR handle,
             const DeviceDispatch& dld) noexcept {
    dld.vkDestroyDescriptorUpdateTemplateKHR(device, handle, nullptr);
}

void Destroy(VkDevice device, VkDeviceMemory handle, const DeviceDispatch& dld) noexcept {
    dld.vkFreeMemory(device, handle, nullptr);
}

void Destroy(VkDevice device, VkFence handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroyFence(device, handle, nullptr);
}

void Destroy(VkDevice device, VkFramebuffer handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroyFramebuffer(device, handle, nullptr);
}

void Destroy(VkDevice device, VkImage handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroyImage(device, handle, nullptr);
}

void Destroy(VkDevice device, VkImageView handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroyImageView(device, handle, nullptr);
}

void Destroy(VkDevice device, VkPipeline handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroyPipeline(device, handle, nullptr);
}

void Destroy(VkDevice device, VkPipelineLayout handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroyPipelineLayout(device, handle, nullptr);
}

void Destroy(VkDevice device, VkQueryPool handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroyQueryPool(device, handle, nullptr);
}

void Destroy(VkDevice device, VkRenderPass handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroyRenderPass(device, handle, nullptr);
}

void Destroy(VkDevice device, VkSampler handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroySampler(device, handle, nullptr);
}

void Destroy(VkDevice device, VkSwapchainKHR handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroySwapchainKHR(device, handle, nullptr);
}

void Destroy(VkDevice device, VkSemaphore handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroySemaphore(device, handle, nullptr);
}

void Destroy(VkDevice device, VkShaderModule handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroyShaderModule(device, handle, nullptr);
}

void Destroy(VkInstance instance, VkDebugUtilsMessengerEXT handle,
             const InstanceDispatch& dld) noexcept {
    dld.vkDestroyDebugUtilsMessengerEXT(instance, handle, nullptr);
}

void Destroy(VkInstance instance, VkSurfaceKHR handle, const InstanceDispatch& dld) noexcept {
    dld.vkDestroySurfaceKHR(instance, handle, nullptr);
}

VkResult Free(VkDevice device, VkDescriptorPool handle, Span<VkDescriptorSet> sets,
              const DeviceDispatch& dld) noexcept {
    return dld.vkFreeDescriptorSets(device, handle, sets.size(), sets.data());
}

VkResult Free(VkDevice device, VkCommandPool handle, Span<VkCommandBuffer> buffers,
              const DeviceDispatch& dld) noexcept {
    dld.vkFreeCommandBuffers(device, handle, buffers.size(), buffers.data());
    return VK_SUCCESS;
}

} // namespace Vulkan::vk
