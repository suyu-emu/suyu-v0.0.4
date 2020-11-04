// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <bitset>
#include <chrono>
#include <optional>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

#include "common/assert.h"
#include "core/settings.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/wrapper.h"

namespace Vulkan {

namespace {

namespace Alternatives {

constexpr std::array Depth24UnormS8_UINT{
    VK_FORMAT_D32_SFLOAT_S8_UINT,
    VK_FORMAT_D16_UNORM_S8_UINT,
    VkFormat{},
};

constexpr std::array Depth16UnormS8_UINT{
    VK_FORMAT_D24_UNORM_S8_UINT,
    VK_FORMAT_D32_SFLOAT_S8_UINT,
    VkFormat{},
};

} // namespace Alternatives

constexpr std::array REQUIRED_EXTENSIONS{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_MAINTENANCE1_EXTENSION_NAME,
    VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME,
    VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
    VK_KHR_16BIT_STORAGE_EXTENSION_NAME,
    VK_KHR_8BIT_STORAGE_EXTENSION_NAME,
    VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME,
    VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_EXTENSION_NAME,
    VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
    VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME,
    VK_EXT_SHADER_SUBGROUP_BALLOT_EXTENSION_NAME,
    VK_EXT_SHADER_SUBGROUP_VOTE_EXTENSION_NAME,
    VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME,
};

template <typename T>
void SetNext(void**& next, T& data) {
    *next = &data;
    next = &data.pNext;
}

constexpr const VkFormat* GetFormatAlternatives(VkFormat format) {
    switch (format) {
    case VK_FORMAT_D24_UNORM_S8_UINT:
        return Alternatives::Depth24UnormS8_UINT.data();
    case VK_FORMAT_D16_UNORM_S8_UINT:
        return Alternatives::Depth16UnormS8_UINT.data();
    default:
        return nullptr;
    }
}

VkFormatFeatureFlags GetFormatFeatures(VkFormatProperties properties, FormatType format_type) {
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

[[nodiscard]] bool IsRDNA(std::string_view device_name, VkDriverIdKHR driver_id) {
    static constexpr std::array RDNA_DEVICES{
        "5700",
        "5600",
        "5500",
        "5300",
    };
    if (driver_id != VK_DRIVER_ID_AMD_PROPRIETARY_KHR) {
        return false;
    }
    return std::any_of(RDNA_DEVICES.begin(), RDNA_DEVICES.end(), [device_name](const char* name) {
        return device_name.find(name) != std::string_view::npos;
    });
}

std::unordered_map<VkFormat, VkFormatProperties> GetFormatProperties(
    vk::PhysicalDevice physical, const vk::InstanceDispatch& dld) {
    static constexpr std::array formats{
        VK_FORMAT_A8B8G8R8_UNORM_PACK32,
        VK_FORMAT_A8B8G8R8_UINT_PACK32,
        VK_FORMAT_A8B8G8R8_SNORM_PACK32,
        VK_FORMAT_A8B8G8R8_SINT_PACK32,
        VK_FORMAT_A8B8G8R8_SRGB_PACK32,
        VK_FORMAT_B5G6R5_UNORM_PACK16,
        VK_FORMAT_A2B10G10R10_UNORM_PACK32,
        VK_FORMAT_A2B10G10R10_UINT_PACK32,
        VK_FORMAT_A1R5G5B5_UNORM_PACK16,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_FORMAT_R32G32B32A32_SINT,
        VK_FORMAT_R32G32B32A32_UINT,
        VK_FORMAT_R32G32_SFLOAT,
        VK_FORMAT_R32G32_SINT,
        VK_FORMAT_R32G32_UINT,
        VK_FORMAT_R16G16B16A16_SINT,
        VK_FORMAT_R16G16B16A16_UINT,
        VK_FORMAT_R16G16B16A16_SNORM,
        VK_FORMAT_R16G16B16A16_UNORM,
        VK_FORMAT_R16G16_UNORM,
        VK_FORMAT_R16G16_SNORM,
        VK_FORMAT_R16G16_SFLOAT,
        VK_FORMAT_R16_UNORM,
        VK_FORMAT_R16_UINT,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_R8G8_UNORM,
        VK_FORMAT_R8G8_SNORM,
        VK_FORMAT_R8G8_SINT,
        VK_FORMAT_R8G8_UINT,
        VK_FORMAT_R8_UNORM,
        VK_FORMAT_R8_SNORM,
        VK_FORMAT_R8_SINT,
        VK_FORMAT_R8_UINT,
        VK_FORMAT_B10G11R11_UFLOAT_PACK32,
        VK_FORMAT_R32_SFLOAT,
        VK_FORMAT_R32_UINT,
        VK_FORMAT_R32_SINT,
        VK_FORMAT_R16_SFLOAT,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_B8G8R8A8_SRGB,
        VK_FORMAT_R4G4B4A4_UNORM_PACK16,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D16_UNORM,
        VK_FORMAT_D16_UNORM_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
        VK_FORMAT_BC2_UNORM_BLOCK,
        VK_FORMAT_BC3_UNORM_BLOCK,
        VK_FORMAT_BC4_UNORM_BLOCK,
        VK_FORMAT_BC4_SNORM_BLOCK,
        VK_FORMAT_BC5_UNORM_BLOCK,
        VK_FORMAT_BC5_SNORM_BLOCK,
        VK_FORMAT_BC7_UNORM_BLOCK,
        VK_FORMAT_BC6H_UFLOAT_BLOCK,
        VK_FORMAT_BC6H_SFLOAT_BLOCK,
        VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
        VK_FORMAT_BC2_SRGB_BLOCK,
        VK_FORMAT_BC3_SRGB_BLOCK,
        VK_FORMAT_BC7_SRGB_BLOCK,
        VK_FORMAT_ASTC_4x4_SRGB_BLOCK,
        VK_FORMAT_ASTC_8x8_SRGB_BLOCK,
        VK_FORMAT_ASTC_8x5_SRGB_BLOCK,
        VK_FORMAT_ASTC_5x4_SRGB_BLOCK,
        VK_FORMAT_ASTC_5x5_UNORM_BLOCK,
        VK_FORMAT_ASTC_5x5_SRGB_BLOCK,
        VK_FORMAT_ASTC_10x8_UNORM_BLOCK,
        VK_FORMAT_ASTC_10x8_SRGB_BLOCK,
        VK_FORMAT_ASTC_6x6_UNORM_BLOCK,
        VK_FORMAT_ASTC_6x6_SRGB_BLOCK,
        VK_FORMAT_ASTC_10x10_UNORM_BLOCK,
        VK_FORMAT_ASTC_10x10_SRGB_BLOCK,
        VK_FORMAT_ASTC_12x12_UNORM_BLOCK,
        VK_FORMAT_ASTC_12x12_SRGB_BLOCK,
        VK_FORMAT_ASTC_8x6_UNORM_BLOCK,
        VK_FORMAT_ASTC_8x6_SRGB_BLOCK,
        VK_FORMAT_ASTC_6x5_UNORM_BLOCK,
        VK_FORMAT_ASTC_6x5_SRGB_BLOCK,
        VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
    };
    std::unordered_map<VkFormat, VkFormatProperties> format_properties;
    for (const auto format : formats) {
        format_properties.emplace(format, physical.GetFormatProperties(format));
    }
    return format_properties;
}

} // Anonymous namespace

VKDevice::VKDevice(VkInstance instance_, u32 instance_version_, vk::PhysicalDevice physical_,
                   VkSurfaceKHR surface, const vk::InstanceDispatch& dld_)
    : dld{dld_}, physical{physical_}, properties{physical.GetProperties()},
      instance_version{instance_version_}, format_properties{GetFormatProperties(physical, dld)} {
    SetupFamilies(surface);
    SetupFeatures();
}

VKDevice::~VKDevice() = default;

bool VKDevice::Create() {
    const auto queue_cis = GetDeviceQueueCreateInfos();
    const std::vector extensions = LoadExtensions();

    VkPhysicalDeviceFeatures2 features2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = nullptr,
    };
    const void* first_next = &features2;
    void** next = &features2.pNext;

    features2.features = {
        .robustBufferAccess = false,
        .fullDrawIndexUint32 = false,
        .imageCubeArray = false,
        .independentBlend = true,
        .geometryShader = true,
        .tessellationShader = true,
        .sampleRateShading = false,
        .dualSrcBlend = false,
        .logicOp = false,
        .multiDrawIndirect = false,
        .drawIndirectFirstInstance = false,
        .depthClamp = true,
        .depthBiasClamp = true,
        .fillModeNonSolid = false,
        .depthBounds = false,
        .wideLines = false,
        .largePoints = true,
        .alphaToOne = false,
        .multiViewport = true,
        .samplerAnisotropy = true,
        .textureCompressionETC2 = false,
        .textureCompressionASTC_LDR = is_optimal_astc_supported,
        .textureCompressionBC = false,
        .occlusionQueryPrecise = true,
        .pipelineStatisticsQuery = false,
        .vertexPipelineStoresAndAtomics = true,
        .fragmentStoresAndAtomics = true,
        .shaderTessellationAndGeometryPointSize = false,
        .shaderImageGatherExtended = true,
        .shaderStorageImageExtendedFormats = false,
        .shaderStorageImageMultisample = false,
        .shaderStorageImageReadWithoutFormat = is_formatless_image_load_supported,
        .shaderStorageImageWriteWithoutFormat = true,
        .shaderUniformBufferArrayDynamicIndexing = false,
        .shaderSampledImageArrayDynamicIndexing = false,
        .shaderStorageBufferArrayDynamicIndexing = false,
        .shaderStorageImageArrayDynamicIndexing = false,
        .shaderClipDistance = false,
        .shaderCullDistance = false,
        .shaderFloat64 = false,
        .shaderInt64 = false,
        .shaderInt16 = false,
        .shaderResourceResidency = false,
        .shaderResourceMinLod = false,
        .sparseBinding = false,
        .sparseResidencyBuffer = false,
        .sparseResidencyImage2D = false,
        .sparseResidencyImage3D = false,
        .sparseResidency2Samples = false,
        .sparseResidency4Samples = false,
        .sparseResidency8Samples = false,
        .sparseResidency16Samples = false,
        .sparseResidencyAliased = false,
        .variableMultisampleRate = false,
        .inheritedQueries = false,
    };

    VkPhysicalDeviceTimelineSemaphoreFeaturesKHR timeline_semaphore{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR,
        .pNext = nullptr,
        .timelineSemaphore = true,
    };
    SetNext(next, timeline_semaphore);

    VkPhysicalDevice16BitStorageFeaturesKHR bit16_storage{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR,
        .pNext = nullptr,
        .storageBuffer16BitAccess = false,
        .uniformAndStorageBuffer16BitAccess = true,
        .storagePushConstant16 = false,
        .storageInputOutput16 = false,
    };
    SetNext(next, bit16_storage);

    VkPhysicalDevice8BitStorageFeaturesKHR bit8_storage{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES_KHR,
        .pNext = nullptr,
        .storageBuffer8BitAccess = false,
        .uniformAndStorageBuffer8BitAccess = true,
        .storagePushConstant8 = false,
    };
    SetNext(next, bit8_storage);

    VkPhysicalDeviceHostQueryResetFeaturesEXT host_query_reset{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES_EXT,
        .hostQueryReset = true,
    };
    SetNext(next, host_query_reset);

    VkPhysicalDeviceFloat16Int8FeaturesKHR float16_int8;
    if (is_float16_supported) {
        float16_int8 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT16_INT8_FEATURES_KHR,
            .pNext = nullptr,
            .shaderFloat16 = true,
            .shaderInt8 = false,
        };
        SetNext(next, float16_int8);
    } else {
        LOG_INFO(Render_Vulkan, "Device doesn't support float16 natively");
    }

    if (!nv_viewport_swizzle) {
        LOG_INFO(Render_Vulkan, "Device doesn't support viewport swizzles");
    }

    VkPhysicalDeviceUniformBufferStandardLayoutFeaturesKHR std430_layout;
    if (khr_uniform_buffer_standard_layout) {
        std430_layout = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES_KHR,
            .pNext = nullptr,
            .uniformBufferStandardLayout = true,
        };
        SetNext(next, std430_layout);
    } else {
        LOG_INFO(Render_Vulkan, "Device doesn't support packed UBOs");
    }

    VkPhysicalDeviceIndexTypeUint8FeaturesEXT index_type_uint8;
    if (ext_index_type_uint8) {
        index_type_uint8 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT,
            .pNext = nullptr,
            .indexTypeUint8 = true,
        };
        SetNext(next, index_type_uint8);
    } else {
        LOG_INFO(Render_Vulkan, "Device doesn't support uint8 indexes");
    }

    VkPhysicalDeviceTransformFeedbackFeaturesEXT transform_feedback;
    if (ext_transform_feedback) {
        transform_feedback = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT,
            .pNext = nullptr,
            .transformFeedback = true,
            .geometryStreams = true,
        };
        SetNext(next, transform_feedback);
    } else {
        LOG_INFO(Render_Vulkan, "Device doesn't support transform feedbacks");
    }

    VkPhysicalDeviceCustomBorderColorFeaturesEXT custom_border;
    if (ext_custom_border_color) {
        custom_border = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT,
            .pNext = nullptr,
            .customBorderColors = VK_TRUE,
            .customBorderColorWithoutFormat = VK_TRUE,
        };
        SetNext(next, custom_border);
    } else {
        LOG_INFO(Render_Vulkan, "Device doesn't support custom border colors");
    }

    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT dynamic_state;
    if (ext_extended_dynamic_state) {
        dynamic_state = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
            .pNext = nullptr,
            .extendedDynamicState = VK_TRUE,
        };
        SetNext(next, dynamic_state);
    } else {
        LOG_INFO(Render_Vulkan, "Device doesn't support extended dynamic state");
    }

    if (!ext_depth_range_unrestricted) {
        LOG_INFO(Render_Vulkan, "Device doesn't support depth range unrestricted");
    }

    VkDeviceDiagnosticsConfigCreateInfoNV diagnostics_nv;
    if (nv_device_diagnostics_config) {
        nsight_aftermath_tracker.Initialize();

        diagnostics_nv = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_DIAGNOSTICS_CONFIG_CREATE_INFO_NV,
            .pNext = &features2,
            .flags = VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_DEBUG_INFO_BIT_NV |
                     VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_RESOURCE_TRACKING_BIT_NV |
                     VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_AUTOMATIC_CHECKPOINTS_BIT_NV,
        };
        first_next = &diagnostics_nv;
    }

    logical = vk::Device::Create(physical, queue_cis, extensions, first_next, dld);
    if (!logical) {
        LOG_ERROR(Render_Vulkan, "Failed to create logical device");
        return false;
    }

    CollectTelemetryParameters();

    if (ext_extended_dynamic_state && IsRDNA(properties.deviceName, driver_id)) {
        // AMD's proprietary driver supports VK_EXT_extended_dynamic_state but on RDNA devices it
        // seems to cause stability issues
        LOG_WARNING(
            Render_Vulkan,
            "Blacklisting AMD proprietary on RDNA devices from VK_EXT_extended_dynamic_state");
        ext_extended_dynamic_state = false;
    }

    graphics_queue = logical.GetQueue(graphics_family);
    present_queue = logical.GetQueue(present_family);

    use_asynchronous_shaders = Settings::values.use_asynchronous_shaders.GetValue();
    return true;
}

VkFormat VKDevice::GetSupportedFormat(VkFormat wanted_format, VkFormatFeatureFlags wanted_usage,
                                      FormatType format_type) const {
    if (IsFormatSupported(wanted_format, wanted_usage, format_type)) {
        return wanted_format;
    }
    // The wanted format is not supported by hardware, search for alternatives
    const VkFormat* alternatives = GetFormatAlternatives(wanted_format);
    if (alternatives == nullptr) {
        UNREACHABLE_MSG("Format={} with usage={} and type={} has no defined alternatives and host "
                        "hardware does not support it",
                        wanted_format, wanted_usage, format_type);
        return wanted_format;
    }

    std::size_t i = 0;
    for (VkFormat alternative = *alternatives; alternative; alternative = alternatives[++i]) {
        if (!IsFormatSupported(alternative, wanted_usage, format_type)) {
            continue;
        }
        LOG_WARNING(Render_Vulkan,
                    "Emulating format={} with alternative format={} with usage={} and type={}",
                    wanted_format, alternative, wanted_usage, format_type);
        return alternative;
    }

    // No alternatives found, panic
    UNREACHABLE_MSG("Format={} with usage={} and type={} is not supported by the host hardware and "
                    "doesn't support any of the alternatives",
                    wanted_format, wanted_usage, format_type);
    return wanted_format;
}

void VKDevice::ReportLoss() const {
    LOG_CRITICAL(Render_Vulkan, "Device loss occured!");

    // Wait for the log to flush and for Nsight Aftermath to dump the results
    std::this_thread::sleep_for(std::chrono::seconds{3});
}

void VKDevice::SaveShader(const std::vector<u32>& spirv) const {
    nsight_aftermath_tracker.SaveShader(spirv);
}

bool VKDevice::IsOptimalAstcSupported(const VkPhysicalDeviceFeatures& features) const {
    // Disable for now to avoid converting ASTC twice.
    static constexpr std::array astc_formats = {
        VK_FORMAT_ASTC_4x4_UNORM_BLOCK,   VK_FORMAT_ASTC_4x4_SRGB_BLOCK,
        VK_FORMAT_ASTC_5x4_UNORM_BLOCK,   VK_FORMAT_ASTC_5x4_SRGB_BLOCK,
        VK_FORMAT_ASTC_5x5_UNORM_BLOCK,   VK_FORMAT_ASTC_5x5_SRGB_BLOCK,
        VK_FORMAT_ASTC_6x5_UNORM_BLOCK,   VK_FORMAT_ASTC_6x5_SRGB_BLOCK,
        VK_FORMAT_ASTC_6x6_UNORM_BLOCK,   VK_FORMAT_ASTC_6x6_SRGB_BLOCK,
        VK_FORMAT_ASTC_8x5_UNORM_BLOCK,   VK_FORMAT_ASTC_8x5_SRGB_BLOCK,
        VK_FORMAT_ASTC_8x6_UNORM_BLOCK,   VK_FORMAT_ASTC_8x6_SRGB_BLOCK,
        VK_FORMAT_ASTC_8x8_UNORM_BLOCK,   VK_FORMAT_ASTC_8x8_SRGB_BLOCK,
        VK_FORMAT_ASTC_10x5_UNORM_BLOCK,  VK_FORMAT_ASTC_10x5_SRGB_BLOCK,
        VK_FORMAT_ASTC_10x6_UNORM_BLOCK,  VK_FORMAT_ASTC_10x6_SRGB_BLOCK,
        VK_FORMAT_ASTC_10x8_UNORM_BLOCK,  VK_FORMAT_ASTC_10x8_SRGB_BLOCK,
        VK_FORMAT_ASTC_10x10_UNORM_BLOCK, VK_FORMAT_ASTC_10x10_SRGB_BLOCK,
        VK_FORMAT_ASTC_12x10_UNORM_BLOCK, VK_FORMAT_ASTC_12x10_SRGB_BLOCK,
        VK_FORMAT_ASTC_12x12_UNORM_BLOCK, VK_FORMAT_ASTC_12x12_SRGB_BLOCK,
    };
    if (!features.textureCompressionASTC_LDR) {
        return false;
    }
    const auto format_feature_usage{
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_BLIT_SRC_BIT |
        VK_FORMAT_FEATURE_BLIT_DST_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
        VK_FORMAT_FEATURE_TRANSFER_DST_BIT};
    for (const auto format : astc_formats) {
        const auto format_properties{physical.GetFormatProperties(format)};
        if (!(format_properties.optimalTilingFeatures & format_feature_usage)) {
            return false;
        }
    }
    return true;
}

bool VKDevice::IsFormatSupported(VkFormat wanted_format, VkFormatFeatureFlags wanted_usage,
                                 FormatType format_type) const {
    const auto it = format_properties.find(wanted_format);
    if (it == format_properties.end()) {
        UNIMPLEMENTED_MSG("Unimplemented format query={}", wanted_format);
        return true;
    }
    const auto supported_usage = GetFormatFeatures(it->second, format_type);
    return (supported_usage & wanted_usage) == wanted_usage;
}

bool VKDevice::IsSuitable(vk::PhysicalDevice physical, VkSurfaceKHR surface) {
    bool is_suitable = true;
    std::bitset<REQUIRED_EXTENSIONS.size()> available_extensions;

    for (const auto& prop : physical.EnumerateDeviceExtensionProperties()) {
        for (std::size_t i = 0; i < REQUIRED_EXTENSIONS.size(); ++i) {
            if (available_extensions[i]) {
                continue;
            }
            const std::string_view name{prop.extensionName};
            available_extensions[i] = name == REQUIRED_EXTENSIONS[i];
        }
    }
    if (!available_extensions.all()) {
        for (std::size_t i = 0; i < REQUIRED_EXTENSIONS.size(); ++i) {
            if (available_extensions[i]) {
                continue;
            }
            LOG_ERROR(Render_Vulkan, "Missing required extension: {}", REQUIRED_EXTENSIONS[i]);
            is_suitable = false;
        }
    }

    bool has_graphics{}, has_present{};
    const std::vector queue_family_properties = physical.GetQueueFamilyProperties();
    for (u32 i = 0; i < static_cast<u32>(queue_family_properties.size()); ++i) {
        const auto& family = queue_family_properties[i];
        if (family.queueCount == 0) {
            continue;
        }
        has_graphics |= family.queueFlags & VK_QUEUE_GRAPHICS_BIT;
        has_present |= physical.GetSurfaceSupportKHR(i, surface);
    }
    if (!has_graphics || !has_present) {
        LOG_ERROR(Render_Vulkan, "Device lacks a graphics and present queue");
        is_suitable = false;
    }

    // TODO(Rodrigo): Check if the device matches all requeriments.
    const auto properties{physical.GetProperties()};
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

    const auto features{physical.GetFeatures()};
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
        std::make_pair(features.occlusionQueryPrecise, "occlusionQueryPrecise"),
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

std::vector<const char*> VKDevice::LoadExtensions() {
    std::vector<const char*> extensions;
    extensions.reserve(7 + REQUIRED_EXTENSIONS.size());
    extensions.insert(extensions.begin(), REQUIRED_EXTENSIONS.begin(), REQUIRED_EXTENSIONS.end());

    bool has_khr_shader_float16_int8{};
    bool has_ext_subgroup_size_control{};
    bool has_ext_transform_feedback{};
    bool has_ext_custom_border_color{};
    bool has_ext_extended_dynamic_state{};
    for (const VkExtensionProperties& extension : physical.EnumerateDeviceExtensionProperties()) {
        const auto test = [&](std::optional<std::reference_wrapper<bool>> status, const char* name,
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
        test(nv_viewport_swizzle, VK_NV_VIEWPORT_SWIZZLE_EXTENSION_NAME, true);
        test(khr_uniform_buffer_standard_layout,
             VK_KHR_UNIFORM_BUFFER_STANDARD_LAYOUT_EXTENSION_NAME, true);
        test(has_khr_shader_float16_int8, VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME, false);
        test(ext_depth_range_unrestricted, VK_EXT_DEPTH_RANGE_UNRESTRICTED_EXTENSION_NAME, true);
        test(ext_index_type_uint8, VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME, true);
        test(ext_shader_viewport_index_layer, VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME,
             true);
        test(has_ext_transform_feedback, VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME, false);
        test(has_ext_custom_border_color, VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME, false);
        test(has_ext_extended_dynamic_state, VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME, false);
        if (instance_version >= VK_API_VERSION_1_1) {
            test(has_ext_subgroup_size_control, VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME, false);
        }
        if (Settings::values.renderer_debug) {
            test(nv_device_diagnostics_config, VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME,
                 true);
        }
    }

    VkPhysicalDeviceFeatures2KHR features;
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;

    VkPhysicalDeviceProperties2KHR properties;
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;

    if (has_khr_shader_float16_int8) {
        VkPhysicalDeviceFloat16Int8FeaturesKHR float16_int8_features;
        float16_int8_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT16_INT8_FEATURES_KHR;
        float16_int8_features.pNext = nullptr;
        features.pNext = &float16_int8_features;

        physical.GetFeatures2KHR(features);
        is_float16_supported = float16_int8_features.shaderFloat16;
        extensions.push_back(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME);
    }

    if (has_ext_subgroup_size_control) {
        VkPhysicalDeviceSubgroupSizeControlFeaturesEXT subgroup_features;
        subgroup_features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES_EXT;
        subgroup_features.pNext = nullptr;
        features.pNext = &subgroup_features;
        physical.GetFeatures2KHR(features);

        VkPhysicalDeviceSubgroupSizeControlPropertiesEXT subgroup_properties;
        subgroup_properties.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT;
        subgroup_properties.pNext = nullptr;
        properties.pNext = &subgroup_properties;
        physical.GetProperties2KHR(properties);

        is_warp_potentially_bigger = subgroup_properties.maxSubgroupSize > GuestWarpSize;

        if (subgroup_features.subgroupSizeControl &&
            subgroup_properties.minSubgroupSize <= GuestWarpSize &&
            subgroup_properties.maxSubgroupSize >= GuestWarpSize) {
            extensions.push_back(VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME);
            guest_warp_stages = subgroup_properties.requiredSubgroupSizeStages;
        }
    } else {
        is_warp_potentially_bigger = true;
    }

    if (has_ext_transform_feedback) {
        VkPhysicalDeviceTransformFeedbackFeaturesEXT tfb_features;
        tfb_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT;
        tfb_features.pNext = nullptr;
        features.pNext = &tfb_features;
        physical.GetFeatures2KHR(features);

        VkPhysicalDeviceTransformFeedbackPropertiesEXT tfb_properties;
        tfb_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT;
        tfb_properties.pNext = nullptr;
        properties.pNext = &tfb_properties;
        physical.GetProperties2KHR(properties);

        if (tfb_features.transformFeedback && tfb_features.geometryStreams &&
            tfb_properties.maxTransformFeedbackStreams >= 4 &&
            tfb_properties.maxTransformFeedbackBuffers && tfb_properties.transformFeedbackQueries &&
            tfb_properties.transformFeedbackDraw) {
            extensions.push_back(VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME);
            ext_transform_feedback = true;
        }
    }

    if (has_ext_custom_border_color) {
        VkPhysicalDeviceCustomBorderColorFeaturesEXT border_features;
        border_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT;
        border_features.pNext = nullptr;
        features.pNext = &border_features;
        physical.GetFeatures2KHR(features);

        if (border_features.customBorderColors && border_features.customBorderColorWithoutFormat) {
            extensions.push_back(VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME);
            ext_custom_border_color = true;
        }
    }

    if (has_ext_extended_dynamic_state) {
        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT dynamic_state;
        dynamic_state.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT;
        dynamic_state.pNext = nullptr;
        features.pNext = &dynamic_state;
        physical.GetFeatures2KHR(features);

        if (dynamic_state.extendedDynamicState) {
            extensions.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
            ext_extended_dynamic_state = true;
        }
    }

    return extensions;
}

void VKDevice::SetupFamilies(VkSurfaceKHR surface) {
    std::optional<u32> graphics_family_, present_family_;

    const std::vector queue_family_properties = physical.GetQueueFamilyProperties();
    for (u32 i = 0; i < static_cast<u32>(queue_family_properties.size()); ++i) {
        if (graphics_family_ && present_family_)
            break;

        const auto& queue_family = queue_family_properties[i];
        if (queue_family.queueCount == 0)
            continue;

        if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_family_ = i;
        }
        if (physical.GetSurfaceSupportKHR(i, surface)) {
            present_family_ = i;
        }
    }
    ASSERT(graphics_family_ && present_family_);

    graphics_family = *graphics_family_;
    present_family = *present_family_;
}

void VKDevice::SetupFeatures() {
    const auto supported_features{physical.GetFeatures()};
    is_formatless_image_load_supported = supported_features.shaderStorageImageReadWithoutFormat;
    is_optimal_astc_supported = IsOptimalAstcSupported(supported_features);
}

void VKDevice::CollectTelemetryParameters() {
    VkPhysicalDeviceDriverPropertiesKHR driver{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES_KHR,
        .pNext = nullptr,
        .driverID = {},
        .driverName = {},
        .driverInfo = {},
        .conformanceVersion = {},
    };

    VkPhysicalDeviceProperties2KHR device_properties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR,
        .pNext = &driver,
        .properties = {},
    };
    physical.GetProperties2KHR(device_properties);

    driver_id = driver.driverID;
    vendor_name = driver.driverName;

    const std::vector extensions = physical.EnumerateDeviceExtensionProperties();
    reported_extensions.reserve(std::size(extensions));
    for (const auto& extension : extensions) {
        reported_extensions.emplace_back(extension.extensionName);
    }
}

std::vector<VkDeviceQueueCreateInfo> VKDevice::GetDeviceQueueCreateInfos() const {
    static constexpr float QUEUE_PRIORITY = 1.0f;

    std::unordered_set<u32> unique_queue_families{graphics_family, present_family};
    std::vector<VkDeviceQueueCreateInfo> queue_cis;
    queue_cis.reserve(unique_queue_families.size());

    for (const u32 queue_family : unique_queue_families) {
        auto& ci = queue_cis.emplace_back(VkDeviceQueueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = queue_family,
            .queueCount = 1,
            .pQueuePriorities = nullptr,
        });
        ci.pQueuePriorities = &QUEUE_PRIORITY;
    }

    return queue_cis;
}

} // namespace Vulkan
