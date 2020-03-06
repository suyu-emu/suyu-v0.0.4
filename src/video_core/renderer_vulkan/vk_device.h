// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "common/common_types.h"
#include "video_core/renderer_vulkan/declarations.h"

namespace Vulkan {

/// Format usage descriptor.
enum class FormatType { Linear, Optimal, Buffer };

/// Subgroup size of the guest emulated hardware (Nvidia has 32 threads per subgroup).
const u32 GuestWarpSize = 32;

/// Handles data specific to a physical device.
class VKDevice final {
public:
    explicit VKDevice(const vk::DispatchLoaderDynamic& dldi, vk::PhysicalDevice physical,
                      vk::SurfaceKHR surface);
    ~VKDevice();

    /// Initializes the device. Returns true on success.
    bool Create(const vk::DispatchLoaderDynamic& dldi, vk::Instance instance);

    /**
     * Returns a format supported by the device for the passed requeriments.
     * @param wanted_format The ideal format to be returned. It may not be the returned format.
     * @param wanted_usage The usage that must be fulfilled even if the format is not supported.
     * @param format_type Format type usage.
     * @returns A format supported by the device.
     */
    vk::Format GetSupportedFormat(vk::Format wanted_format, vk::FormatFeatureFlags wanted_usage,
                                  FormatType format_type) const;

    /// Reports a device loss.
    void ReportLoss() const;

    /// Returns the dispatch loader with direct function pointers of the device.
    const vk::DispatchLoaderDynamic& GetDispatchLoader() const {
        return dld;
    }

    /// Returns the logical device.
    vk::Device GetLogical() const {
        return logical.get();
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

    /// Returns true if the device is integrated with the host CPU.
    bool IsIntegrated() const {
        return properties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu;
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
    vk::DriverIdKHR GetDriverID() const {
        return driver_id;
    }

    /// Returns uniform buffer alignment requeriment.
    vk::DeviceSize GetUniformBufferAlignment() const {
        return properties.limits.minUniformBufferOffsetAlignment;
    }

    /// Returns storage alignment requeriment.
    vk::DeviceSize GetStorageBufferAlignment() const {
        return properties.limits.minStorageBufferOffsetAlignment;
    }

    /// Returns the maximum range for storage buffers.
    vk::DeviceSize GetMaxStorageBufferRange() const {
        return properties.limits.maxStorageBufferRange;
    }

    /// Returns the maximum size for push constants.
    vk::DeviceSize GetMaxPushConstantsSize() const {
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
    bool IsGuestWarpSizeSupported(vk::ShaderStageFlagBits stage) const {
        return (guest_warp_stages & stage) != vk::ShaderStageFlags{};
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

    /// Returns true if the device supports VK_NV_device_diagnostic_checkpoints.
    bool IsNvDeviceDiagnosticCheckpoints() const {
        return nv_device_diagnostic_checkpoints;
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
    static bool IsSuitable(const vk::DispatchLoaderDynamic& dldi, vk::PhysicalDevice physical,
                           vk::SurfaceKHR surface);

private:
    /// Loads extensions into a vector and stores available ones in this object.
    std::vector<const char*> LoadExtensions(const vk::DispatchLoaderDynamic& dldi);

    /// Sets up queue families.
    void SetupFamilies(const vk::DispatchLoaderDynamic& dldi, vk::SurfaceKHR surface);

    /// Sets up device features.
    void SetupFeatures(const vk::DispatchLoaderDynamic& dldi);

    /// Collects telemetry information from the device.
    void CollectTelemetryParameters();

    /// Returns a list of queue initialization descriptors.
    std::vector<vk::DeviceQueueCreateInfo> GetDeviceQueueCreateInfos() const;

    /// Returns true if ASTC textures are natively supported.
    bool IsOptimalAstcSupported(const vk::PhysicalDeviceFeatures& features,
                                const vk::DispatchLoaderDynamic& dldi) const;

    /// Returns true if a format is supported.
    bool IsFormatSupported(vk::Format wanted_format, vk::FormatFeatureFlags wanted_usage,
                           FormatType format_type) const;

    /// Returns the device properties for Vulkan formats.
    static std::unordered_map<vk::Format, vk::FormatProperties> GetFormatProperties(
        const vk::DispatchLoaderDynamic& dldi, vk::PhysicalDevice physical);

    const vk::PhysicalDevice physical;        ///< Physical device.
    vk::DispatchLoaderDynamic dld;            ///< Device function pointers.
    vk::PhysicalDeviceProperties properties;  ///< Device properties.
    UniqueDevice logical;                     ///< Logical device.
    vk::Queue graphics_queue;                 ///< Main graphics queue.
    vk::Queue present_queue;                  ///< Main present queue.
    u32 graphics_family{};                    ///< Main graphics queue family index.
    u32 present_family{};                     ///< Main present queue family index.
    vk::DriverIdKHR driver_id{};              ///< Driver ID.
    vk::ShaderStageFlags guest_warp_stages{}; ///< Stages where the guest warp size can be forced.ed
    bool is_optimal_astc_supported{};         ///< Support for native ASTC.
    bool is_float16_supported{};              ///< Support for float16 arithmetics.
    bool is_warp_potentially_bigger{};        ///< Host warp size can be bigger than guest.
    bool is_formatless_image_load_supported{}; ///< Support for shader image read without format.
    bool khr_uniform_buffer_standard_layout{}; ///< Support for std430 on UBOs.
    bool ext_index_type_uint8{};               ///< Support for VK_EXT_index_type_uint8.
    bool ext_depth_range_unrestricted{};       ///< Support for VK_EXT_depth_range_unrestricted.
    bool ext_shader_viewport_index_layer{};    ///< Support for VK_EXT_shader_viewport_index_layer.
    bool nv_device_diagnostic_checkpoints{};   ///< Support for VK_NV_device_diagnostic_checkpoints.

    // Telemetry parameters
    std::string vendor_name;                      ///< Device's driver name.
    std::vector<std::string> reported_extensions; ///< Reported Vulkan extensions.

    /// Format properties dictionary.
    std::unordered_map<vk::Format, vk::FormatProperties> format_properties;
};

} // namespace Vulkan
