// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <bitset>
#include <chrono>
#include <cstdlib>
#include <optional>
#include <set>
#include <string_view>
#include <thread>
#include <vector>
#include "common/assert.h"
#include "core/settings.h"
#include "video_core/renderer_vulkan/declarations.h"
#include "video_core/renderer_vulkan/vk_device.h"

namespace Vulkan {

namespace {

namespace Alternatives {

constexpr std::array Depth24UnormS8Uint = {vk::Format::eD32SfloatS8Uint,
                                           vk::Format::eD16UnormS8Uint, vk::Format{}};
constexpr std::array Depth16UnormS8Uint = {vk::Format::eD24UnormS8Uint,
                                           vk::Format::eD32SfloatS8Uint, vk::Format{}};

} // namespace Alternatives

template <typename T>
void SetNext(void**& next, T& data) {
    *next = &data;
    next = &data.pNext;
}

template <typename T>
T GetFeatures(vk::PhysicalDevice physical, const vk::DispatchLoaderDynamic& dldi) {
    vk::PhysicalDeviceFeatures2 features;
    T extension_features;
    features.pNext = &extension_features;
    physical.getFeatures2(&features, dldi);
    return extension_features;
}

template <typename T>
T GetProperties(vk::PhysicalDevice physical, const vk::DispatchLoaderDynamic& dldi) {
    vk::PhysicalDeviceProperties2 properties;
    T extension_properties;
    properties.pNext = &extension_properties;
    physical.getProperties2(&properties, dldi);
    return extension_properties;
}

constexpr const vk::Format* GetFormatAlternatives(vk::Format format) {
    switch (format) {
    case vk::Format::eD24UnormS8Uint:
        return Alternatives::Depth24UnormS8Uint.data();
    case vk::Format::eD16UnormS8Uint:
        return Alternatives::Depth16UnormS8Uint.data();
    default:
        return nullptr;
    }
}

vk::FormatFeatureFlags GetFormatFeatures(vk::FormatProperties properties, FormatType format_type) {
    switch (format_type) {
    case FormatType::Linear:
        return properties.linearTilingFeatures;
    case FormatType::Optimal:
        return properties.optimalTilingFeatures;
    case FormatType::Buffer:
        return properties.bufferFeatures;
    default:
        return {};
    }
}

} // Anonymous namespace

VKDevice::VKDevice(const vk::DispatchLoaderDynamic& dldi, vk::PhysicalDevice physical,
                   vk::SurfaceKHR surface)
    : physical{physical}, properties{physical.getProperties(dldi)},
      format_properties{GetFormatProperties(dldi, physical)} {
    SetupFamilies(dldi, surface);
    SetupFeatures(dldi);
}

VKDevice::~VKDevice() = default;

bool VKDevice::Create(const vk::DispatchLoaderDynamic& dldi, vk::Instance instance) {
    const auto queue_cis = GetDeviceQueueCreateInfos();
    const std::vector extensions = LoadExtensions(dldi);

    vk::PhysicalDeviceFeatures2 features2;
    void** next = &features2.pNext;
    auto& features = features2.features;
    features.vertexPipelineStoresAndAtomics = true;
    features.independentBlend = true;
    features.depthClamp = true;
    features.samplerAnisotropy = true;
    features.largePoints = true;
    features.multiViewport = true;
    features.depthBiasClamp = true;
    features.geometryShader = true;
    features.tessellationShader = true;
    features.fragmentStoresAndAtomics = true;
    features.shaderImageGatherExtended = true;
    features.shaderStorageImageWriteWithoutFormat = true;
    features.textureCompressionASTC_LDR = is_optimal_astc_supported;

    vk::PhysicalDevice16BitStorageFeaturesKHR bit16_storage;
    bit16_storage.uniformAndStorageBuffer16BitAccess = true;
    SetNext(next, bit16_storage);

    vk::PhysicalDevice8BitStorageFeaturesKHR bit8_storage;
    bit8_storage.uniformAndStorageBuffer8BitAccess = true;
    SetNext(next, bit8_storage);

    vk::PhysicalDeviceFloat16Int8FeaturesKHR float16_int8;
    if (is_float16_supported) {
        float16_int8.shaderFloat16 = true;
        SetNext(next, float16_int8);
    } else {
        LOG_INFO(Render_Vulkan, "Device doesn't support float16 natively");
    }

    vk::PhysicalDeviceUniformBufferStandardLayoutFeaturesKHR std430_layout;
    if (khr_uniform_buffer_standard_layout) {
        std430_layout.uniformBufferStandardLayout = true;
        SetNext(next, std430_layout);
    } else {
        LOG_INFO(Render_Vulkan, "Device doesn't support packed UBOs");
    }

    vk::PhysicalDeviceIndexTypeUint8FeaturesEXT index_type_uint8;
    if (ext_index_type_uint8) {
        index_type_uint8.indexTypeUint8 = true;
        SetNext(next, index_type_uint8);
    } else {
        LOG_INFO(Render_Vulkan, "Device doesn't support uint8 indexes");
    }

    if (!ext_depth_range_unrestricted) {
        LOG_INFO(Render_Vulkan, "Device doesn't support depth range unrestricted");
    }

    vk::DeviceCreateInfo device_ci({}, static_cast<u32>(queue_cis.size()), queue_cis.data(), 0,
                                   nullptr, static_cast<u32>(extensions.size()), extensions.data(),
                                   nullptr);
    device_ci.pNext = &features2;

    vk::Device dummy_logical;
    if (physical.createDevice(&device_ci, nullptr, &dummy_logical, dldi) != vk::Result::eSuccess) {
        LOG_CRITICAL(Render_Vulkan, "Logical device failed to be created!");
        return false;
    }

    dld.init(instance, dldi.vkGetInstanceProcAddr, dummy_logical, dldi.vkGetDeviceProcAddr);
    logical = UniqueDevice(
        dummy_logical, vk::ObjectDestroy<vk::NoParent, vk::DispatchLoaderDynamic>(nullptr, dld));

    CollectTelemetryParameters();

    graphics_queue = logical->getQueue(graphics_family, 0, dld);
    present_queue = logical->getQueue(present_family, 0, dld);
    return true;
}

vk::Format VKDevice::GetSupportedFormat(vk::Format wanted_format,
                                        vk::FormatFeatureFlags wanted_usage,
                                        FormatType format_type) const {
    if (IsFormatSupported(wanted_format, wanted_usage, format_type)) {
        return wanted_format;
    }
    // The wanted format is not supported by hardware, search for alternatives
    const vk::Format* alternatives = GetFormatAlternatives(wanted_format);
    if (alternatives == nullptr) {
        UNREACHABLE_MSG("Format={} with usage={} and type={} has no defined alternatives and host "
                        "hardware does not support it",
                        vk::to_string(wanted_format), vk::to_string(wanted_usage),
                        static_cast<u32>(format_type));
        return wanted_format;
    }

    std::size_t i = 0;
    for (vk::Format alternative = alternatives[0]; alternative != vk::Format{};
         alternative = alternatives[++i]) {
        if (!IsFormatSupported(alternative, wanted_usage, format_type)) {
            continue;
        }
        LOG_WARNING(Render_Vulkan,
                    "Emulating format={} with alternative format={} with usage={} and type={}",
                    static_cast<u32>(wanted_format), static_cast<u32>(alternative),
                    static_cast<u32>(wanted_usage), static_cast<u32>(format_type));
        return alternative;
    }

    // No alternatives found, panic
    UNREACHABLE_MSG("Format={} with usage={} and type={} is not supported by the host hardware and "
                    "doesn't support any of the alternatives",
                    static_cast<u32>(wanted_format), static_cast<u32>(wanted_usage),
                    static_cast<u32>(format_type));
    return wanted_format;
}

void VKDevice::ReportLoss() const {
    LOG_CRITICAL(Render_Vulkan, "Device loss occured!");

    // Wait some time to let the log flush
    std::this_thread::sleep_for(std::chrono::seconds{1});

    if (!nv_device_diagnostic_checkpoints) {
        return;
    }

    [[maybe_unused]] const std::vector data = graphics_queue.getCheckpointDataNV(dld);
    // Catch here in debug builds (or with optimizations disabled) the last graphics pipeline to be
    // executed. It can be done on a debugger by evaluating the expression:
    // *(VKGraphicsPipeline*)data[0]
}

bool VKDevice::IsOptimalAstcSupported(const vk::PhysicalDeviceFeatures& features,
                                      const vk::DispatchLoaderDynamic& dldi) const {
    // Disable for now to avoid converting ASTC twice.
    return false;
    static constexpr std::array astc_formats = {
        vk::Format::eAstc4x4SrgbBlock,    vk::Format::eAstc8x8SrgbBlock,
        vk::Format::eAstc8x5SrgbBlock,    vk::Format::eAstc5x4SrgbBlock,
        vk::Format::eAstc5x5UnormBlock,   vk::Format::eAstc5x5SrgbBlock,
        vk::Format::eAstc10x8UnormBlock,  vk::Format::eAstc10x8SrgbBlock,
        vk::Format::eAstc6x6UnormBlock,   vk::Format::eAstc6x6SrgbBlock,
        vk::Format::eAstc10x10UnormBlock, vk::Format::eAstc10x10SrgbBlock,
        vk::Format::eAstc12x12UnormBlock, vk::Format::eAstc12x12SrgbBlock,
        vk::Format::eAstc8x6UnormBlock,   vk::Format::eAstc8x6SrgbBlock,
        vk::Format::eAstc6x5UnormBlock,   vk::Format::eAstc6x5SrgbBlock};
    if (!features.textureCompressionASTC_LDR) {
        return false;
    }
    const auto format_feature_usage{
        vk::FormatFeatureFlagBits::eSampledImage | vk::FormatFeatureFlagBits::eBlitSrc |
        vk::FormatFeatureFlagBits::eBlitDst | vk::FormatFeatureFlagBits::eTransferSrc |
        vk::FormatFeatureFlagBits::eTransferDst};
    for (const auto format : astc_formats) {
        const auto format_properties{physical.getFormatProperties(format, dldi)};
        if (!(format_properties.optimalTilingFeatures & format_feature_usage)) {
            return false;
        }
    }
    return true;
}

bool VKDevice::IsFormatSupported(vk::Format wanted_format, vk::FormatFeatureFlags wanted_usage,
                                 FormatType format_type) const {
    const auto it = format_properties.find(wanted_format);
    if (it == format_properties.end()) {
        UNIMPLEMENTED_MSG("Unimplemented format query={}", vk::to_string(wanted_format));
        return true;
    }
    const auto supported_usage = GetFormatFeatures(it->second, format_type);
    return (supported_usage & wanted_usage) == wanted_usage;
}

bool VKDevice::IsSuitable(const vk::DispatchLoaderDynamic& dldi, vk::PhysicalDevice physical,
                          vk::SurfaceKHR surface) {
    bool is_suitable = true;

    constexpr std::array required_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_16BIT_STORAGE_EXTENSION_NAME,
        VK_KHR_8BIT_STORAGE_EXTENSION_NAME,
        VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME,
        VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME,
        VK_EXT_SHADER_SUBGROUP_BALLOT_EXTENSION_NAME,
        VK_EXT_SHADER_SUBGROUP_VOTE_EXTENSION_NAME,
    };
    std::bitset<required_extensions.size()> available_extensions{};

    for (const auto& prop : physical.enumerateDeviceExtensionProperties(nullptr, dldi)) {
        for (std::size_t i = 0; i < required_extensions.size(); ++i) {
            if (available_extensions[i]) {
                continue;
            }
            available_extensions[i] =
                required_extensions[i] == std::string_view{prop.extensionName};
        }
    }
    if (!available_extensions.all()) {
        for (std::size_t i = 0; i < required_extensions.size(); ++i) {
            if (available_extensions[i]) {
                continue;
            }
            LOG_ERROR(Render_Vulkan, "Missing required extension: {}", required_extensions[i]);
            is_suitable = false;
        }
    }

    bool has_graphics{}, has_present{};
    const auto queue_family_properties = physical.getQueueFamilyProperties(dldi);
    for (u32 i = 0; i < static_cast<u32>(queue_family_properties.size()); ++i) {
        const auto& family = queue_family_properties[i];
        if (family.queueCount == 0) {
            continue;
        }
        has_graphics |=
            (family.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlagBits>(0);
        has_present |= physical.getSurfaceSupportKHR(i, surface, dldi) != 0;
    }
    if (!has_graphics || !has_present) {
        LOG_ERROR(Render_Vulkan, "Device lacks a graphics and present queue");
        is_suitable = false;
    }

    // TODO(Rodrigo): Check if the device matches all requeriments.
    const auto properties{physical.getProperties(dldi)};
    const auto& limits{properties.limits};

    constexpr u32 required_ubo_size = 65536;
    if (limits.maxUniformBufferRange < required_ubo_size) {
        LOG_ERROR(Render_Vulkan, "Device UBO size {} is too small, {} is required",
                  limits.maxUniformBufferRange, required_ubo_size);
        is_suitable = false;
    }

    constexpr u32 required_num_viewports = 16;
    if (limits.maxViewports < required_num_viewports) {
        LOG_INFO(Render_Vulkan, "Device number of viewports {} is too small, {} is required",
                 limits.maxViewports, required_num_viewports);
        is_suitable = false;
    }

    const auto features{physical.getFeatures(dldi)};
    const std::array feature_report = {
        std::make_pair(features.vertexPipelineStoresAndAtomics, "vertexPipelineStoresAndAtomics"),
        std::make_pair(features.independentBlend, "independentBlend"),
        std::make_pair(features.depthClamp, "depthClamp"),
        std::make_pair(features.samplerAnisotropy, "samplerAnisotropy"),
        std::make_pair(features.largePoints, "largePoints"),
        std::make_pair(features.multiViewport, "multiViewport"),
        std::make_pair(features.depthBiasClamp, "depthBiasClamp"),
        std::make_pair(features.geometryShader, "geometryShader"),
        std::make_pair(features.tessellationShader, "tessellationShader"),
        std::make_pair(features.fragmentStoresAndAtomics, "fragmentStoresAndAtomics"),
        std::make_pair(features.shaderImageGatherExtended, "shaderImageGatherExtended"),
        std::make_pair(features.shaderStorageImageWriteWithoutFormat,
                       "shaderStorageImageWriteWithoutFormat"),
    };
    for (const auto& [supported, name] : feature_report) {
        if (supported) {
            continue;
        }
        LOG_ERROR(Render_Vulkan, "Missing required feature: {}", name);
        is_suitable = false;
    }

    if (!is_suitable) {
        LOG_ERROR(Render_Vulkan, "{} is not suitable", properties.deviceName);
    }

    return is_suitable;
}

std::vector<const char*> VKDevice::LoadExtensions(const vk::DispatchLoaderDynamic& dldi) {
    std::vector<const char*> extensions;
    const auto Test = [&](const vk::ExtensionProperties& extension,
                          std::optional<std::reference_wrapper<bool>> status, const char* name,
                          bool push) {
        if (extension.extensionName != std::string_view(name)) {
            return;
        }
        if (push) {
            extensions.push_back(name);
        }
        if (status) {
            status->get() = true;
        }
    };

    extensions.reserve(13);
    extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    extensions.push_back(VK_KHR_16BIT_STORAGE_EXTENSION_NAME);
    extensions.push_back(VK_KHR_8BIT_STORAGE_EXTENSION_NAME);
    extensions.push_back(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME);
    extensions.push_back(VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME);
    extensions.push_back(VK_EXT_SHADER_SUBGROUP_BALLOT_EXTENSION_NAME);
    extensions.push_back(VK_EXT_SHADER_SUBGROUP_VOTE_EXTENSION_NAME);

    [[maybe_unused]] const bool nsight =
        std::getenv("NVTX_INJECTION64_PATH") || std::getenv("NSIGHT_LAUNCHED");
    bool khr_shader_float16_int8{};
    bool ext_subgroup_size_control{};
    for (const auto& extension : physical.enumerateDeviceExtensionProperties(nullptr, dldi)) {
        Test(extension, khr_uniform_buffer_standard_layout,
             VK_KHR_UNIFORM_BUFFER_STANDARD_LAYOUT_EXTENSION_NAME, true);
        Test(extension, khr_shader_float16_int8, VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME, false);
        Test(extension, ext_depth_range_unrestricted,
             VK_EXT_DEPTH_RANGE_UNRESTRICTED_EXTENSION_NAME, true);
        Test(extension, ext_index_type_uint8, VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME, true);
        Test(extension, ext_shader_viewport_index_layer,
             VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME, true);
        Test(extension, ext_subgroup_size_control, VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME,
             false);
        if (Settings::values.renderer_debug) {
            Test(extension, nv_device_diagnostic_checkpoints,
                 VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME, true);
        }
    }

    if (khr_shader_float16_int8) {
        is_float16_supported =
            GetFeatures<vk::PhysicalDeviceFloat16Int8FeaturesKHR>(physical, dldi).shaderFloat16;
        extensions.push_back(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME);
    }

    if (ext_subgroup_size_control) {
        const auto features =
            GetFeatures<vk::PhysicalDeviceSubgroupSizeControlFeaturesEXT>(physical, dldi);
        const auto properties =
            GetProperties<vk::PhysicalDeviceSubgroupSizeControlPropertiesEXT>(physical, dldi);

        is_warp_potentially_bigger = properties.maxSubgroupSize > GuestWarpSize;

        if (features.subgroupSizeControl && properties.minSubgroupSize <= GuestWarpSize &&
            properties.maxSubgroupSize >= GuestWarpSize) {
            extensions.push_back(VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME);
            guest_warp_stages = properties.requiredSubgroupSizeStages;
        }
    } else {
        is_warp_potentially_bigger = true;
    }

    return extensions;
}

void VKDevice::SetupFamilies(const vk::DispatchLoaderDynamic& dldi, vk::SurfaceKHR surface) {
    std::optional<u32> graphics_family_, present_family_;

    const auto queue_family_properties = physical.getQueueFamilyProperties(dldi);
    for (u32 i = 0; i < static_cast<u32>(queue_family_properties.size()); ++i) {
        if (graphics_family_ && present_family_)
            break;

        const auto& queue_family = queue_family_properties[i];
        if (queue_family.queueCount == 0)
            continue;

        if (queue_family.queueFlags & vk::QueueFlagBits::eGraphics)
            graphics_family_ = i;
        if (physical.getSurfaceSupportKHR(i, surface, dldi))
            present_family_ = i;
    }
    ASSERT(graphics_family_ && present_family_);

    graphics_family = *graphics_family_;
    present_family = *present_family_;
}

void VKDevice::SetupFeatures(const vk::DispatchLoaderDynamic& dldi) {
    const auto supported_features{physical.getFeatures(dldi)};
    is_optimal_astc_supported = IsOptimalAstcSupported(supported_features, dldi);
}

void VKDevice::CollectTelemetryParameters() {
    const auto driver = GetProperties<vk::PhysicalDeviceDriverPropertiesKHR>(physical, dld);
    driver_id = driver.driverID;
    vendor_name = driver.driverName;

    const auto extensions = physical.enumerateDeviceExtensionProperties(nullptr, dld);
    reported_extensions.reserve(std::size(extensions));
    for (const auto& extension : extensions) {
        reported_extensions.push_back(extension.extensionName);
    }
}

std::vector<vk::DeviceQueueCreateInfo> VKDevice::GetDeviceQueueCreateInfos() const {
    static const float QUEUE_PRIORITY = 1.0f;

    std::set<u32> unique_queue_families = {graphics_family, present_family};
    std::vector<vk::DeviceQueueCreateInfo> queue_cis;

    for (u32 queue_family : unique_queue_families)
        queue_cis.push_back({{}, queue_family, 1, &QUEUE_PRIORITY});

    return queue_cis;
}

std::unordered_map<vk::Format, vk::FormatProperties> VKDevice::GetFormatProperties(
    const vk::DispatchLoaderDynamic& dldi, vk::PhysicalDevice physical) {
    static constexpr std::array formats{vk::Format::eA8B8G8R8UnormPack32,
                                        vk::Format::eA8B8G8R8UintPack32,
                                        vk::Format::eA8B8G8R8SnormPack32,
                                        vk::Format::eA8B8G8R8SrgbPack32,
                                        vk::Format::eB5G6R5UnormPack16,
                                        vk::Format::eA2B10G10R10UnormPack32,
                                        vk::Format::eA1R5G5B5UnormPack16,
                                        vk::Format::eR32G32B32A32Sfloat,
                                        vk::Format::eR32G32B32A32Uint,
                                        vk::Format::eR32G32Sfloat,
                                        vk::Format::eR32G32Uint,
                                        vk::Format::eR16G16B16A16Uint,
                                        vk::Format::eR16G16B16A16Unorm,
                                        vk::Format::eR16G16Unorm,
                                        vk::Format::eR16G16Snorm,
                                        vk::Format::eR16G16Sfloat,
                                        vk::Format::eR16Unorm,
                                        vk::Format::eR8G8B8A8Srgb,
                                        vk::Format::eR8G8Unorm,
                                        vk::Format::eR8G8Snorm,
                                        vk::Format::eR8Unorm,
                                        vk::Format::eR8Uint,
                                        vk::Format::eB10G11R11UfloatPack32,
                                        vk::Format::eR32Sfloat,
                                        vk::Format::eR32Uint,
                                        vk::Format::eR16Sfloat,
                                        vk::Format::eR16G16B16A16Sfloat,
                                        vk::Format::eB8G8R8A8Unorm,
                                        vk::Format::eR4G4B4A4UnormPack16,
                                        vk::Format::eD32Sfloat,
                                        vk::Format::eD16Unorm,
                                        vk::Format::eD16UnormS8Uint,
                                        vk::Format::eD24UnormS8Uint,
                                        vk::Format::eD32SfloatS8Uint,
                                        vk::Format::eBc1RgbaUnormBlock,
                                        vk::Format::eBc2UnormBlock,
                                        vk::Format::eBc3UnormBlock,
                                        vk::Format::eBc4UnormBlock,
                                        vk::Format::eBc5UnormBlock,
                                        vk::Format::eBc5SnormBlock,
                                        vk::Format::eBc7UnormBlock,
                                        vk::Format::eBc6HUfloatBlock,
                                        vk::Format::eBc6HSfloatBlock,
                                        vk::Format::eBc1RgbaSrgbBlock,
                                        vk::Format::eBc3SrgbBlock,
                                        vk::Format::eBc7SrgbBlock,
                                        vk::Format::eAstc4x4SrgbBlock,
                                        vk::Format::eAstc8x8SrgbBlock,
                                        vk::Format::eAstc8x5SrgbBlock,
                                        vk::Format::eAstc5x4SrgbBlock,
                                        vk::Format::eAstc5x5UnormBlock,
                                        vk::Format::eAstc5x5SrgbBlock,
                                        vk::Format::eAstc10x8UnormBlock,
                                        vk::Format::eAstc10x8SrgbBlock,
                                        vk::Format::eAstc6x6UnormBlock,
                                        vk::Format::eAstc6x6SrgbBlock,
                                        vk::Format::eAstc10x10UnormBlock,
                                        vk::Format::eAstc10x10SrgbBlock,
                                        vk::Format::eAstc12x12UnormBlock,
                                        vk::Format::eAstc12x12SrgbBlock,
                                        vk::Format::eAstc8x6UnormBlock,
                                        vk::Format::eAstc8x6SrgbBlock,
                                        vk::Format::eAstc6x5UnormBlock,
                                        vk::Format::eAstc6x5SrgbBlock,
                                        vk::Format::eE5B9G9R9UfloatPack32};
    std::unordered_map<vk::Format, vk::FormatProperties> format_properties;
    for (const auto format : formats) {
        format_properties.emplace(format, physical.getFormatProperties(format, dldi));
    }
    return format_properties;
}

} // namespace Vulkan
