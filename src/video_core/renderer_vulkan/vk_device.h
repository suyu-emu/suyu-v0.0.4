// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "common/common_types.h"
#include "video_core/renderer_vulkan/nsight_aftermath_tracker.h"
#include "video_core/renderer_vulkan/wrapper.h"

namespace Vulkan {

/// Format usage descriptor.
enum class FormatType { Linear, Optimal, Buffer };

/// Subgroup size of the guest emulated hardware (Nvidia has 32 threads per subgroup).
const u32 GuestWarpSize = 32;

/// Handles data specific to a physical device.
class VKDevice final {
public:
    explicit VKDevice(VkInstance instance, vk::PhysicalDevice physical, VkSurfaceKHR surface,
                      const vk::InstanceDispatch& dld);
    ~VKDevice();

    /// Initializes the device. Returns true on success.
    bool Create();

    /**
     * Returns a format supported by the device for the passed requeriments.
     * @param wanted_format The ideal format to be returned. It may not be the returned format.
     * @param wanted_usage The usage that must be fulfilled even if the format is not supported.
     * @param format_type Format type usage.
     * @returns A format supported by the device.
     */
    VkFormat GetSupportedFormat(VkFormat wanted_format, VkFormatFeatureFlags wanted_usage,
                                FormatType format_type) const;

    /// Reports a device loss.
    void ReportLoss() const;

    /// Reports a shader to Nsight Aftermath.
    void SaveShader(const std::vector<u32>& spirv) const;

    /// Returns the dispatch loader with direct function pointers of the device.
    const vk::DeviceDispatch& GetDispatchLoader() const {
        return dld;
    }

    /// Returns the logical device.
    const vk::Device& GetLogical() const {
        return logical;
    }

    /// Returns the physical device.
    vk::PhysicalDevice GetPhysical() const {
        return physical;
    }

    /// Returns the main graphics queue.
    vk::Queue GetGraphicsQueue() const {
        return graphics_queue;
    }

    /// Returns the main present queue.
    vk::Queue GetPresentQueue() const {
        return present_queue;
    }

    /// Returns main graphics queue family index.
    u32 GetGraphicsFamily() const {
        return graphics_family;
    }

    /// Returns main present queue family index.
    u32 GetPresentFamily() const {
        return present_family;
    }

    /// Returns the current Vulkan API version provided in Vulkan-formatted version numbers.
    u32 GetApiVersion() const {
        return properties.apiVersion;
    }

    /// Returns the current driver version provided in Vulkan-formatted version numbers.
    u32 GetDriverVersion() const {
        return properties.driverVersion;
    }

    /// Returns the device name.
    std::string_view GetModelName() const {
        return properties.deviceName;
    }

    /// Returns the driver ID.
    VkDriverIdKHR GetDriverID() const {
        return driver_id;
    }

    /// Returns uniform buffer alignment requeriment.
    VkDeviceSize GetUniformBufferAlignment() const {
        return properties.limits.minUniformBufferOffsetAlignment;
    }

    /// Returns storage alignment requeriment.
    VkDeviceSize GetStorageBufferAlignment() const {
        return properties.limits.minStorageBufferOffsetAlignment;
    }

    /// Returns the maximum range for storage buffers.
    VkDeviceSize GetMaxStorageBufferRange() const {
        return properties.limits.maxStorageBufferRange;
    }

    /// Returns the maximum size for push constants.
    VkDeviceSize GetMaxPushConstantsSize() const {
        return properties.limits.maxPushConstantsSize;
    }

    /// Returns true if ASTC is natively supported.
    bool IsOptimalAstcSupported() const {
        return is_optimal_astc_supported;
    }

    /// Returns true if the device supports float16 natively
    bool IsFloat16Supported() const {
        return is_float16_supported;
    }

    /// Returns true if the device warp size can potentially be bigger than guest's warp size.
    bool IsWarpSizePotentiallyBiggerThanGuest() const {
        return is_warp_potentially_bigger;
    }

    /// Returns true if the device can be forced to use the guest warp size.
    bool IsGuestWarpSizeSupported(VkShaderStageFlagBits stage) const {
        return guest_warp_stages & stage;
    }

    /// Returns true if formatless image load is supported.
    bool IsFormatlessImageLoadSupported() const {
        return is_formatless_image_load_supported;
    }

    /// Returns true if the device supports VK_EXT_scalar_block_layout.
    bool IsKhrUniformBufferStandardLayoutSupported() const {
        return khr_uniform_buffer_standard_layout;
    }

    /// Returns true if the device supports VK_EXT_index_type_uint8.
    bool IsExtIndexTypeUint8Supported() const {
        return ext_index_type_uint8;
    }

    /// Returns true if the device supports VK_EXT_depth_range_unrestricted.
    bool IsExtDepthRangeUnrestrictedSupported() const {
        return ext_depth_range_unrestricted;
    }

    /// Returns true if the device supports VK_EXT_shader_viewport_index_layer.
    bool IsExtShaderViewportIndexLayerSupported() const {
        return ext_shader_viewport_index_layer;
    }

    /// Returns true if the device supports VK_EXT_transform_feedback.
    bool IsExtTransformFeedbackSupported() const {
        return ext_transform_feedback;
    }

    /// Returns the vendor name reported from Vulkan.
    std::string_view GetVendorName() const {
        return vendor_name;
    }

    /// Returns the list of available extensions.
    const std::vector<std::string>& GetAvailableExtensions() const {
        return reported_extensions;
    }

    /// Checks if the physical device is suitable.
    static bool IsSuitable(vk::PhysicalDevice physical, VkSurfaceKHR surface);

private:
    /// Loads extensions into a vector and stores available ones in this object.
    std::vector<const char*> LoadExtensions();

    /// Sets up queue families.
    void SetupFamilies(VkSurfaceKHR surface);

    /// Sets up device features.
    void SetupFeatures();

    /// Collects telemetry information from the device.
    void CollectTelemetryParameters();

    /// Returns a list of queue initialization descriptors.
    std::vector<VkDeviceQueueCreateInfo> GetDeviceQueueCreateInfos() const;

    /// Returns true if ASTC textures are natively supported.
    bool IsOptimalAstcSupported(const VkPhysicalDeviceFeatures& features) const;

    /// Returns true if a format is supported.
    bool IsFormatSupported(VkFormat wanted_format, VkFormatFeatureFlags wanted_usage,
                           FormatType format_type) const;

    vk::DeviceDispatch dld;                 ///< Device function pointers.
    vk::PhysicalDevice physical;            ///< Physical device.
    VkPhysicalDeviceProperties properties;  ///< Device properties.
    vk::Device logical;                     ///< Logical device.
    vk::Queue graphics_queue;               ///< Main graphics queue.
    vk::Queue present_queue;                ///< Main present queue.
    u32 graphics_family{};                  ///< Main graphics queue family index.
    u32 present_family{};                   ///< Main present queue family index.
    VkDriverIdKHR driver_id{};              ///< Driver ID.
    VkShaderStageFlags guest_warp_stages{}; ///< Stages where the guest warp size can be forced.ed
    bool is_optimal_astc_supported{};       ///< Support for native ASTC.
    bool is_float16_supported{};            ///< Support for float16 arithmetics.
    bool is_warp_potentially_bigger{};      ///< Host warp size can be bigger than guest.
    bool is_formatless_image_load_supported{}; ///< Support for shader image read without format.
    bool khr_uniform_buffer_standard_layout{}; ///< Support for std430 on UBOs.
    bool ext_index_type_uint8{};               ///< Support for VK_EXT_index_type_uint8.
    bool ext_depth_range_unrestricted{};       ///< Support for VK_EXT_depth_range_unrestricted.
    bool ext_shader_viewport_index_layer{};    ///< Support for VK_EXT_shader_viewport_index_layer.
    bool ext_transform_feedback{};             ///< Support for VK_EXT_transform_feedback.
    bool nv_device_diagnostics_config{};       ///< Support for VK_NV_device_diagnostics_config.

    // Telemetry parameters
    std::string vendor_name;                      ///< Device's driver name.
    std::vector<std::string> reported_extensions; ///< Reported Vulkan extensions.

    /// Format properties dictionary.
    std::unordered_map<VkFormat, VkFormatProperties> format_properties;

    /// Nsight Aftermath GPU crash tracker
    NsightAftermathTracker nsight_aftermath_tracker;
};

} // namespace Vulkan
