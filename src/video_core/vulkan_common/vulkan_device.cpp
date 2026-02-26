// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <bitset>
#include <chrono>
#include <optional>
#include <thread>
#include <ankerl/unordered_dense.h>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include "common/assert.h"
#include "common/literals.h"
#include <ranges>
#include "common/settings.h"
#include "common/settings_enums.h"
#include "video_core/vulkan_common/nsight_aftermath_tracker.h"
#include "video_core/vulkan_common/vma.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"
#include "video_core/gpu_logging/gpu_logging.h"

#if defined(ANDROID) && defined(ARCHITECTURE_arm64)
#include <adrenotools/bcenabler.h>
#include <android/api-level.h>
#endif

namespace Vulkan {
using namespace Common::Literals;
namespace {
namespace Alternatives {
constexpr std::array STENCIL8_UINT{
    VK_FORMAT_D16_UNORM_S8_UINT,
    VK_FORMAT_D24_UNORM_S8_UINT,
    VK_FORMAT_D32_SFLOAT_S8_UINT,
    VK_FORMAT_UNDEFINED,
};

constexpr std::array DEPTH24_UNORM_STENCIL8_UINT{
    VK_FORMAT_D32_SFLOAT_S8_UINT,
    VK_FORMAT_D16_UNORM_S8_UINT,
    VK_FORMAT_UNDEFINED,
};

constexpr std::array DEPTH24_UNORM_DONTCARE8{
    VK_FORMAT_D32_SFLOAT,
    VK_FORMAT_D16_UNORM,
    VK_FORMAT_UNDEFINED,
};

constexpr std::array DEPTH16_UNORM_STENCIL8_UINT{
    VK_FORMAT_D24_UNORM_S8_UINT,
    VK_FORMAT_D32_SFLOAT_S8_UINT,
    VK_FORMAT_UNDEFINED,
};

constexpr std::array B5G6R5_UNORM_PACK16{
    VK_FORMAT_R5G6B5_UNORM_PACK16,
    VK_FORMAT_UNDEFINED,
};

constexpr std::array R4G4_UNORM_PACK8{
    VK_FORMAT_R8_UNORM,
    VK_FORMAT_UNDEFINED,
};

constexpr std::array R16G16B16_SFLOAT{
    VK_FORMAT_R16G16B16A16_SFLOAT,
    VK_FORMAT_UNDEFINED,
};

constexpr std::array R16G16B16_SSCALED{
    VK_FORMAT_R16G16B16A16_SSCALED,
    VK_FORMAT_UNDEFINED,
};

constexpr std::array R8G8B8_SSCALED{
    VK_FORMAT_R8G8B8A8_SSCALED,
    VK_FORMAT_UNDEFINED,
};

constexpr std::array VK_FORMAT_R32G32B32_SFLOAT{
    VK_FORMAT_R32G32B32A32_SFLOAT,
    VK_FORMAT_UNDEFINED,
};

constexpr std::array VK_FORMAT_A4B4G4R4_UNORM_PACK16{
    VK_FORMAT_R4G4B4A4_UNORM_PACK16,
    VK_FORMAT_UNDEFINED,
};

} // namespace Alternatives

template <typename T>
void SetNext(void**& next, T& data) {
    *next = &data;
    next = &data.pNext;
}

constexpr const VkFormat* GetFormatAlternatives(VkFormat format) {
    switch (format) {
    case VK_FORMAT_S8_UINT:
        return Alternatives::STENCIL8_UINT.data();
    case VK_FORMAT_D24_UNORM_S8_UINT:
        return Alternatives::DEPTH24_UNORM_STENCIL8_UINT.data();
    case VK_FORMAT_X8_D24_UNORM_PACK32:
        return Alternatives::DEPTH24_UNORM_DONTCARE8.data();
    case VK_FORMAT_D16_UNORM_S8_UINT:
        return Alternatives::DEPTH16_UNORM_STENCIL8_UINT.data();
    case VK_FORMAT_B5G6R5_UNORM_PACK16:
        return Alternatives::B5G6R5_UNORM_PACK16.data();
    case VK_FORMAT_R4G4_UNORM_PACK8:
        return Alternatives::R4G4_UNORM_PACK8.data();
    case VK_FORMAT_R16G16B16_SFLOAT:
        return Alternatives::R16G16B16_SFLOAT.data();
    case VK_FORMAT_R16G16B16_SSCALED:
        return Alternatives::R16G16B16_SSCALED.data();
    case VK_FORMAT_R8G8B8_SSCALED:
        return Alternatives::R8G8B8_SSCALED.data();
    case VK_FORMAT_R32G32B32_SFLOAT:
        return Alternatives::VK_FORMAT_R32G32B32_SFLOAT.data();
    case VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT:
        return Alternatives::VK_FORMAT_A4B4G4R4_UNORM_PACK16.data();
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

ankerl::unordered_dense::map<VkFormat, VkFormatProperties> GetFormatProperties(vk::PhysicalDevice physical) {
    static constexpr std::array formats{
        VK_FORMAT_A1R5G5B5_UNORM_PACK16,
        VK_FORMAT_A2B10G10R10_SINT_PACK32,
        VK_FORMAT_A2B10G10R10_SNORM_PACK32,
        VK_FORMAT_A2B10G10R10_SSCALED_PACK32,
        VK_FORMAT_A2B10G10R10_UINT_PACK32,
        VK_FORMAT_A2B10G10R10_UNORM_PACK32,
        VK_FORMAT_A2B10G10R10_USCALED_PACK32,
        VK_FORMAT_A2R10G10B10_UNORM_PACK32,
        VK_FORMAT_A8B8G8R8_SINT_PACK32,
        VK_FORMAT_A8B8G8R8_SNORM_PACK32,
        VK_FORMAT_A8B8G8R8_SRGB_PACK32,
        VK_FORMAT_A8B8G8R8_UINT_PACK32,
        VK_FORMAT_A8B8G8R8_UNORM_PACK32,
        VK_FORMAT_ASTC_10x10_SRGB_BLOCK,
        VK_FORMAT_ASTC_10x10_UNORM_BLOCK,
        VK_FORMAT_ASTC_10x5_SRGB_BLOCK,
        VK_FORMAT_ASTC_10x5_UNORM_BLOCK,
        VK_FORMAT_ASTC_10x6_SRGB_BLOCK,
        VK_FORMAT_ASTC_10x6_UNORM_BLOCK,
        VK_FORMAT_ASTC_10x8_SRGB_BLOCK,
        VK_FORMAT_ASTC_10x8_UNORM_BLOCK,
        VK_FORMAT_ASTC_12x10_SRGB_BLOCK,
        VK_FORMAT_ASTC_12x10_UNORM_BLOCK,
        VK_FORMAT_ASTC_12x12_SRGB_BLOCK,
        VK_FORMAT_ASTC_12x12_UNORM_BLOCK,
        VK_FORMAT_ASTC_4x4_SRGB_BLOCK,
        VK_FORMAT_ASTC_4x4_UNORM_BLOCK,
        VK_FORMAT_ASTC_5x4_SRGB_BLOCK,
        VK_FORMAT_ASTC_5x4_UNORM_BLOCK,
        VK_FORMAT_ASTC_5x5_SRGB_BLOCK,
        VK_FORMAT_ASTC_5x5_UNORM_BLOCK,
        VK_FORMAT_ASTC_6x5_SRGB_BLOCK,
        VK_FORMAT_ASTC_6x5_UNORM_BLOCK,
        VK_FORMAT_ASTC_6x6_SRGB_BLOCK,
        VK_FORMAT_ASTC_6x6_UNORM_BLOCK,
        VK_FORMAT_ASTC_8x5_SRGB_BLOCK,
        VK_FORMAT_ASTC_8x5_UNORM_BLOCK,
        VK_FORMAT_ASTC_8x6_SRGB_BLOCK,
        VK_FORMAT_ASTC_8x6_UNORM_BLOCK,
        VK_FORMAT_ASTC_8x8_SRGB_BLOCK,
        VK_FORMAT_ASTC_8x8_UNORM_BLOCK,
        VK_FORMAT_B10G11R11_UFLOAT_PACK32,
        VK_FORMAT_B4G4R4A4_UNORM_PACK16,
        VK_FORMAT_B5G5R5A1_UNORM_PACK16,
        VK_FORMAT_B5G6R5_UNORM_PACK16,
        VK_FORMAT_B8G8R8A8_SRGB,
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
        VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
        VK_FORMAT_BC2_SRGB_BLOCK,
        VK_FORMAT_BC2_UNORM_BLOCK,
        VK_FORMAT_BC3_SRGB_BLOCK,
        VK_FORMAT_BC3_UNORM_BLOCK,
        VK_FORMAT_BC4_SNORM_BLOCK,
        VK_FORMAT_BC4_UNORM_BLOCK,
        VK_FORMAT_BC5_SNORM_BLOCK,
        VK_FORMAT_BC5_UNORM_BLOCK,
        VK_FORMAT_BC6H_SFLOAT_BLOCK,
        VK_FORMAT_BC6H_UFLOAT_BLOCK,
        VK_FORMAT_BC7_SRGB_BLOCK,
        VK_FORMAT_BC7_UNORM_BLOCK,
        VK_FORMAT_D16_UNORM,
        VK_FORMAT_D16_UNORM_S8_UINT,
        VK_FORMAT_X8_D24_UNORM_PACK32,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_FORMAT_R16G16B16A16_SINT,
        VK_FORMAT_R16G16B16A16_SNORM,
        VK_FORMAT_R16G16B16A16_SSCALED,
        VK_FORMAT_R16G16B16A16_UINT,
        VK_FORMAT_R16G16B16A16_UNORM,
        VK_FORMAT_R16G16B16A16_USCALED,
        VK_FORMAT_R16G16B16_SFLOAT,
        VK_FORMAT_R16G16B16_SINT,
        VK_FORMAT_R16G16B16_SNORM,
        VK_FORMAT_R16G16B16_SSCALED,
        VK_FORMAT_R16G16B16_UINT,
        VK_FORMAT_R16G16B16_UNORM,
        VK_FORMAT_R16G16B16_USCALED,
        VK_FORMAT_R16G16_SFLOAT,
        VK_FORMAT_R16G16_SINT,
        VK_FORMAT_R16G16_SNORM,
        VK_FORMAT_R16G16_SSCALED,
        VK_FORMAT_R16G16_UINT,
        VK_FORMAT_R16G16_UNORM,
        VK_FORMAT_R16G16_USCALED,
        VK_FORMAT_R16_SFLOAT,
        VK_FORMAT_R16_SINT,
        VK_FORMAT_R16_SNORM,
        VK_FORMAT_R16_SSCALED,
        VK_FORMAT_R16_UINT,
        VK_FORMAT_R16_UNORM,
        VK_FORMAT_R16_USCALED,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_FORMAT_R32G32B32A32_SINT,
        VK_FORMAT_R32G32B32A32_UINT,
        VK_FORMAT_R32G32B32_SFLOAT,
        VK_FORMAT_R32G32B32_SINT,
        VK_FORMAT_R32G32B32_UINT,
        VK_FORMAT_R32G32_SFLOAT,
        VK_FORMAT_R32G32_SINT,
        VK_FORMAT_R32G32_UINT,
        VK_FORMAT_R32_SFLOAT,
        VK_FORMAT_R32_SINT,
        VK_FORMAT_R32_UINT,
        VK_FORMAT_R4G4B4A4_UNORM_PACK16,
        VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT,
        VK_FORMAT_R4G4_UNORM_PACK8,
        VK_FORMAT_R5G5B5A1_UNORM_PACK16,
        VK_FORMAT_R5G6B5_UNORM_PACK16,
        VK_FORMAT_R8G8B8A8_SINT,
        VK_FORMAT_R8G8B8A8_SNORM,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_R8G8B8A8_SSCALED,
        VK_FORMAT_R8G8B8A8_UINT,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R8G8B8A8_USCALED,
        VK_FORMAT_R8G8B8_SINT,
        VK_FORMAT_R8G8B8_SNORM,
        VK_FORMAT_R8G8B8_SSCALED,
        VK_FORMAT_R8G8B8_UINT,
        VK_FORMAT_R8G8B8_UNORM,
        VK_FORMAT_R8G8B8_USCALED,
        VK_FORMAT_R8G8_SINT,
        VK_FORMAT_R8G8_SNORM,
        VK_FORMAT_R8G8_SSCALED,
        VK_FORMAT_R8G8_UINT,
        VK_FORMAT_R8G8_UNORM,
        VK_FORMAT_R8G8_USCALED,
        VK_FORMAT_R8_SINT,
        VK_FORMAT_R8_SNORM,
        VK_FORMAT_R8_SSCALED,
        VK_FORMAT_R8_UINT,
        VK_FORMAT_R8_UNORM,
        VK_FORMAT_R8_USCALED,
        VK_FORMAT_S8_UINT,
    };
    ankerl::unordered_dense::map<VkFormat, VkFormatProperties> format_properties;
    for (const auto format : formats) {
        format_properties.emplace(format, physical.GetFormatProperties(format));
    }
    return format_properties;
}

#if defined(ANDROID) && defined(ARCHITECTURE_arm64)
void OverrideBcnFormats(ankerl::unordered_dense::map<VkFormat, VkFormatProperties>& format_properties) {
    // These properties are extracted from Adreno driver 512.687.0
    constexpr VkFormatFeatureFlags tiling_features{VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
                                                   VK_FORMAT_FEATURE_BLIT_SRC_BIT |
                                                   VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
                                                   VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
                                                   VK_FORMAT_FEATURE_TRANSFER_DST_BIT};

    constexpr VkFormatFeatureFlags buffer_features{VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT};

    static constexpr std::array bcn_formats{
        VK_FORMAT_BC1_RGBA_SRGB_BLOCK, VK_FORMAT_BC1_RGBA_UNORM_BLOCK, VK_FORMAT_BC2_SRGB_BLOCK,
        VK_FORMAT_BC2_UNORM_BLOCK,     VK_FORMAT_BC3_SRGB_BLOCK,       VK_FORMAT_BC3_UNORM_BLOCK,
        VK_FORMAT_BC4_SNORM_BLOCK,     VK_FORMAT_BC4_UNORM_BLOCK,      VK_FORMAT_BC5_SNORM_BLOCK,
        VK_FORMAT_BC5_UNORM_BLOCK,     VK_FORMAT_BC6H_SFLOAT_BLOCK,    VK_FORMAT_BC6H_UFLOAT_BLOCK,
        VK_FORMAT_BC7_SRGB_BLOCK,      VK_FORMAT_BC7_UNORM_BLOCK,
    };

    for (const auto format : bcn_formats) {
        format_properties[format].linearTilingFeatures = tiling_features;
        format_properties[format].optimalTilingFeatures = tiling_features;
        format_properties[format].bufferFeatures = buffer_features;
    }
}
#endif

NvidiaArchitecture GetNvidiaArchitecture(vk::PhysicalDevice physical,
                                        const std::set<std::string, std::less<>>& exts) {
    VkPhysicalDeviceProperties2 physical_properties{};
    physical_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    physical_properties.pNext = nullptr;

    if (exts.contains(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME)) {
        VkPhysicalDeviceFragmentShadingRatePropertiesKHR shading_rate_props{};
        shading_rate_props.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR;
        physical_properties.pNext = &shading_rate_props;
        physical.GetProperties2(physical_properties);
        if (shading_rate_props.primitiveFragmentShadingRateWithMultipleViewports) {
            return NvidiaArchitecture::Arch_AmpereOrNewer;
        }
        return NvidiaArchitecture::Arch_Turing;
    }

    if (exts.contains(VK_EXT_BLEND_OPERATION_ADVANCED_EXTENSION_NAME)) {
        VkPhysicalDeviceBlendOperationAdvancedPropertiesEXT advanced_blending_props{};
        advanced_blending_props.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_PROPERTIES_EXT;
        physical_properties.pNext = &advanced_blending_props;
        physical.GetProperties2(physical_properties);
        if (advanced_blending_props.advancedBlendMaxColorAttachments == 1) {
            return NvidiaArchitecture::Arch_Maxwell;
        }

        if (exts.contains(VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME)) {
            VkPhysicalDeviceConservativeRasterizationPropertiesEXT conservative_raster_props{};
            conservative_raster_props.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONSERVATIVE_RASTERIZATION_PROPERTIES_EXT;
            physical_properties.pNext = &conservative_raster_props;
            physical.GetProperties2(physical_properties);
            if (conservative_raster_props.degenerateLinesRasterized) {
                return NvidiaArchitecture::Arch_Volta;
            }
            return NvidiaArchitecture::Arch_Pascal;
        }
    }

    return NvidiaArchitecture::Arch_KeplerOrOlder;
}

std::vector<const char*> ExtensionListForVulkan(
    const std::set<std::string, std::less<>>& extensions) {
    std::vector<const char*> output;
    output.reserve(extensions.size());
    for (const auto& extension : extensions) {
        output.push_back(extension.c_str());
    }
    return output;
}

} // Anonymous namespace

void Device::RemoveExtension(bool& extension, const std::string& extension_name) {
    extension = false;
    loaded_extensions.erase(extension_name);
}

void Device::RemoveExtensionIfUnsuitable(bool is_suitable, const std::string& extension_name) {
    if (loaded_extensions.contains(extension_name) && !is_suitable) {
        LOG_WARNING(Render_Vulkan, "Removing unsuitable extension {}", extension_name);
        this->RemoveExtension(is_suitable, extension_name);
    }
}

template <typename Feature>
void Device::RemoveExtensionFeature(bool& extension, Feature& feature,
                                    const std::string& extension_name) {
    // Unload extension.
    this->RemoveExtension(extension, extension_name);

    // Save sType and pNext for chain.
    VkStructureType sType = feature.sType;
    void* pNext = feature.pNext;

    // Clear feature struct and restore chain.
    feature = {};
    feature.sType = sType;
    feature.pNext = pNext;
}

template <typename Feature>
void Device::RemoveExtensionFeatureIfUnsuitable(bool is_suitable, Feature& feature,
                                                const std::string& extension_name) {
    if (loaded_extensions.contains(extension_name) && !is_suitable) {
        LOG_WARNING(Render_Vulkan, "Removing features for unsuitable extension {}", extension_name);
        this->RemoveExtensionFeature(is_suitable, feature, extension_name);
    }
}

Device::Device(VkInstance instance_, vk::PhysicalDevice physical_, VkSurfaceKHR surface,
               const vk::InstanceDispatch& dld_)
    : instance{instance_}, dld{dld_}, physical{physical_},
    format_properties(GetFormatProperties(physical)) {
    // Get suitability and device properties.
    const bool is_suitable = GetSuitability(surface != nullptr);

    const VkDriverId driver_id = properties.driver.driverID;

    const bool is_radv = driver_id == VK_DRIVER_ID_MESA_RADV;
    const bool is_amd_driver =
        driver_id == VK_DRIVER_ID_AMD_PROPRIETARY || driver_id == VK_DRIVER_ID_AMD_OPEN_SOURCE;
    const bool is_amd = is_amd_driver || is_radv;

    const bool is_intel_windows = driver_id == VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS;
    const bool is_intel_anv = driver_id == VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA;

    const bool is_nvidia = driver_id == VK_DRIVER_ID_NVIDIA_PROPRIETARY;
    const bool is_mvk = driver_id == VK_DRIVER_ID_MOLTENVK;
    const bool is_qualcomm = driver_id == VK_DRIVER_ID_QUALCOMM_PROPRIETARY;
    const bool is_turnip = driver_id == VK_DRIVER_ID_MESA_TURNIP;

    if (!is_suitable)
        LOG_WARNING(Render_Vulkan, "Unsuitable driver - continuing anyways");

    if (is_nvidia) {
        nvidia_arch = GetNvidiaArchitecture(physical, supported_extensions);
    }

    SetupFamilies(surface);
    const auto queue_cis = GetDeviceQueueCreateInfos();

    // GetSuitability has already configured the linked list of features for us.
    // Reuse it here.
    const void* first_next = &features2;

    VkDeviceDiagnosticsConfigCreateInfoNV diagnostics_nv{};
    const bool use_diagnostics_nv = Settings::values.enable_nsight_aftermath && extensions.device_diagnostics_config;
    if (use_diagnostics_nv) {
        nsight_aftermath_tracker = std::make_unique<NsightAftermathTracker>();

        diagnostics_nv = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_DIAGNOSTICS_CONFIG_CREATE_INFO_NV,
            .pNext = &features2,
            .flags = VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_DEBUG_INFO_BIT_NV |
                     VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_RESOURCE_TRACKING_BIT_NV |
                     VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_AUTOMATIC_CHECKPOINTS_BIT_NV,
        };
        first_next = &diagnostics_nv;
    }

    VkPhysicalDeviceDescriptorIndexingFeaturesEXT descriptor_indexing{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT,
        .pNext = use_diagnostics_nv ? static_cast<void*>(&diagnostics_nv) : static_cast<void*>(&features2),
        .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
        .descriptorBindingPartiallyBound = VK_TRUE,
        .descriptorBindingVariableDescriptorCount = VK_TRUE,
    };

    if (extensions.descriptor_indexing && Settings::values.descriptor_indexing.GetValue()) {
        first_next = &descriptor_indexing;
    }

    is_blit_depth24_stencil8_supported = TestDepthStencilBlits(VK_FORMAT_D24_UNORM_S8_UINT);
    is_blit_depth32_stencil8_supported = TestDepthStencilBlits(VK_FORMAT_D32_SFLOAT_S8_UINT);
    is_optimal_astc_supported = ComputeIsOptimalAstcSupported();
    is_warp_potentially_bigger = !extensions.subgroup_size_control ||
                                 properties.subgroup_size_control.maxSubgroupSize > GuestWarpSize;

    is_integrated = properties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
    is_virtual = properties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU;
    is_non_gpu = properties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_OTHER ||
                 properties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU;

    supports_d24_depth =
        IsFormatSupported(VK_FORMAT_D24_UNORM_S8_UINT,
                          VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, FormatType::Optimal);

    supports_conditional_barriers = !(is_intel_anv || is_intel_windows);

    CollectPhysicalMemoryInfo();
    CollectToolingInfo();

    if (is_qualcomm) {
        // Qualcomm Adreno GPUs doesn't handle scaled vertex attributes; keep emulation enabled
        must_emulate_scaled_formats = true;
        LOG_WARNING(Render_Vulkan,
                    "Qualcomm drivers require scaled vertex format emulation; forcing fallback");

        LOG_WARNING(Render_Vulkan,
                    "Disabling shader float controls and 64-bit integer features on Qualcomm proprietary drivers");
        RemoveExtension(extensions.shader_float_controls, VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
        RemoveExtensionFeature(extensions.shader_atomic_int64, features.shader_atomic_int64,
                               VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME);
        features.shader_atomic_int64.shaderBufferInt64Atomics = false;
        features.shader_atomic_int64.shaderSharedInt64Atomics = false;
        features.features.shaderInt64 = false;

#if defined(ANDROID) && defined(ARCHITECTURE_arm64)
        // BCn patching only safe on Android 9+ (API 28+). Older versions crash on driver load.
        const auto major = (properties.properties.driverVersion >> 24) << 2;
        const auto minor = (properties.properties.driverVersion >> 12) & 0xFFFU;
        const auto vendor = properties.properties.vendorID;
        const auto patch_status = adrenotools_get_bcn_type(major, minor, vendor);
        const int api_level = android_get_device_api_level();

        bool should_patch_bcn = api_level >= 28;
        const bool bcn_debug_override = Settings::values.patch_old_qcom_drivers.GetValue();
        if (bcn_debug_override != should_patch_bcn) {
            LOG_WARNING(Render_Vulkan,
                "BCn patch debug override active: {} (auto-detected: {})",
                bcn_debug_override, should_patch_bcn);
            should_patch_bcn = bcn_debug_override;
        }

        if (patch_status == ADRENOTOOLS_BCN_PATCH) {
            if (should_patch_bcn) {
                LOG_INFO(Render_Vulkan,
                    "Patching Adreno driver to support BCn texture formats "
                    "(Android API {}, Driver {}.{})", api_level, major, minor);
                if (adrenotools_patch_bcn(
                        reinterpret_cast<void*>(dld.vkGetPhysicalDeviceFormatProperties))) {
                    OverrideBcnFormats(format_properties);
                } else {
                    LOG_ERROR(Render_Vulkan, "BCn patch failed! Driver code may now crash");
                }
            } else {
                LOG_WARNING(Render_Vulkan,
                    "BCn texture patching skipped for stability (Android API {} < 28). "
                    "Driver version {}.{} would support patching, but may crash on older Android.",
                    api_level, major, minor);
            }
        } else if (patch_status == ADRENOTOOLS_BCN_BLOB) {
            LOG_INFO(Render_Vulkan, "Adreno driver supports BCn textures natively (no patch needed)");
        } else {
            LOG_INFO(Render_Vulkan,
                "Adreno driver does not support BCn texture patching (Android API {}, Driver {}.{})",
                api_level, major, minor);
        }
#endif
    }

    if (is_nvidia) {
        const u32 nv_major_version = (properties.properties.driverVersion >> 22) & 0x3ff;
        const auto arch = GetNvidiaArch();
        if (arch >= NvidiaArchitecture::Arch_AmpereOrNewer) {
            LOG_WARNING(Render_Vulkan, "Ampere and newer have broken float16 math");
            features.shader_float16_int8.shaderFloat16 = false;
        }

        if (nv_major_version >= 510) {
            LOG_WARNING(Render_Vulkan,
                        "NVIDIA Drivers >= 510 do not support MSAA->MSAA image blits. "
                        "MSAA scaling will use 3D helpers. MSAA resolves work normally.");
            cant_blit_msaa = true;
        }

        // Mali/ NVIDIA proprietary drivers: Shader stencil export not supported
        // Use hardware depth/stencil blits instead when available
        if (!extensions.shader_stencil_export) {
            LOG_INFO(Render_Vulkan,
                     "NVIDIA: VK_EXT_shader_stencil_export not supported, using hardware blits "
                     "for depth/stencil operations");
            LOG_INFO(Render_Vulkan, "  D24S8 hardware blit support: {}",
                     is_blit_depth24_stencil8_supported);
            LOG_INFO(Render_Vulkan, "  D32S8 hardware blit support: {}",
                     is_blit_depth32_stencil8_supported);

            if (!is_blit_depth24_stencil8_supported && !is_blit_depth32_stencil8_supported) {
                LOG_WARNING(Render_Vulkan,
                            "NVIDIA: Neither shader export nor hardware blits available for "
                            "depth/stencil. Performance may be degraded.");
            }
        }
    }

    sets_per_pool = 64;
    if (is_amd_driver) {
        // AMD drivers need a higher amount of Sets per Pool in certain circumstances like in XC2.
        sets_per_pool = 96;
        // Disable VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT on AMD GCN4 and lower as it is broken.
        if (!features.shader_float16_int8.shaderFloat16) {
            LOG_WARNING(Render_Vulkan,
                        "AMD GCN4 and earlier have broken VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT");
            has_broken_cube_compatibility = true;
        }
    }

    if (is_qualcomm) {
        const size_t sampler_limit = properties.properties.limits.maxSamplerAllocationCount;
        if (sampler_limit > 0) {
            constexpr size_t MIN_SAMPLER_BUDGET = 1024U;
            const size_t reserved = sampler_limit / 4U;
            const size_t derived_budget =
                (std::max)(MIN_SAMPLER_BUDGET, sampler_limit - reserved);
            sampler_heap_budget = derived_budget;
            LOG_WARNING(Render_Vulkan,
                        "Qualcomm driver reports max {} samplers; reserving {} (25%) and "
                        "allowing Eden to use {} (75%) to avoid heap exhaustion",
                        sampler_limit, reserved, sampler_heap_budget);
        }
    }

    if (extensions.sampler_filter_minmax && is_amd) {
        // Disable ext_sampler_filter_minmax on AMD GCN4 and lower as it is broken.
        if (!features.shader_float16_int8.shaderFloat16) {
            LOG_WARNING(Render_Vulkan,
                        "AMD GCN4 and earlier have broken VK_EXT_sampler_filter_minmax");
            RemoveExtension(extensions.sampler_filter_minmax,
                            VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME);
        }
    }

    if (features.shader_float16_int8.shaderFloat16 && is_intel_windows) {
        // Intel's compiler crashes when using fp16 on Astral Chain, disable it for the time being.
        LOG_WARNING(Render_Vulkan, "Intel has broken float16 math");
        features.shader_float16_int8.shaderFloat16 = false;
    }

    if (is_intel_windows) {
        LOG_WARNING(Render_Vulkan,
                    "Intel proprietary drivers do not support MSAA->MSAA image blits. "
                    "MSAA scaling will use 3D helpers. MSAA resolves work normally.");
        cant_blit_msaa = true;
    }

    has_broken_compute =
        CheckBrokenCompute(properties.driver.driverID, properties.properties.driverVersion) &&
        !Settings::values.enable_compute_pipelines.GetValue();

    if (is_mvk) {
        LOG_WARNING(Render_Vulkan,
                    "MVK driver breaks when using more than 16 vertex attributes/bindings");
        properties.properties.limits.maxVertexInputAttributes =
            (std::min)(properties.properties.limits.maxVertexInputAttributes, 16U);
        properties.properties.limits.maxVertexInputBindings =
            (std::min)(properties.properties.limits.maxVertexInputBindings, 16U);
    }

    if (is_turnip || is_qualcomm) {
        LOG_WARNING(Render_Vulkan, "Driver requires higher-than-reported binding limits");
        properties.properties.limits.maxVertexInputBindings = 32;
    }

    const auto dyna_state = Settings::values.dyna_state.GetValue();

    // Base dynamic states (VIEWPORT, SCISSOR, DEPTH_BIAS, etc.) are ALWAYS active in vk_graphics_pipeline.cpp
    // This slider controls EXTENDED dynamic states with accumulative levels per Vulkan specs:
    //   Level 0 = Core Dynamic States only (Vulkan 1.0)
    //   Level 1 = Core + VK_EXT_extended_dynamic_state
    //   Level 2 = Core + VK_EXT_extended_dynamic_state + VK_EXT_extended_dynamic_state2
    //   Level 3 = Core + VK_EXT_extended_dynamic_state + VK_EXT_extended_dynamic_state2 + VK_EXT_extended_dynamic_state3

    switch (dyna_state) {
    case Settings::ExtendedDynamicState::Disabled:
        // Level 0: Disable all extended dynamic state extensions
        RemoveExtensionFeature(extensions.extended_dynamic_state, features.extended_dynamic_state,
                              VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
        RemoveExtensionFeature(extensions.extended_dynamic_state2, features.extended_dynamic_state2,
                              VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME);
        RemoveExtensionFeature(extensions.extended_dynamic_state3, features.extended_dynamic_state3,
                              VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME);
        dynamic_state3_blending = false;
        dynamic_state3_enables = false;
        break;
    case Settings::ExtendedDynamicState::EDS1:
        // Level 1: Enable EDS1, disable EDS2 and EDS3
        RemoveExtensionFeature(extensions.extended_dynamic_state2, features.extended_dynamic_state2,
                              VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME);
        RemoveExtensionFeature(extensions.extended_dynamic_state3, features.extended_dynamic_state3,
                              VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME);
        dynamic_state3_blending = false;
        dynamic_state3_enables = false;
        break;
    case Settings::ExtendedDynamicState::EDS2:
        // Level 2: Enable EDS1 + EDS2, disable EDS3
        RemoveExtensionFeature(extensions.extended_dynamic_state3, features.extended_dynamic_state3,
                              VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME);
        dynamic_state3_blending = false;
        dynamic_state3_enables = false;
        break;
    case Settings::ExtendedDynamicState::EDS3:
    default:
        // Level 3: Enable all (EDS1 + EDS2 + EDS3)
        break;
    }

    // VK_EXT_vertex_input_dynamic_state is independent from EDS
    // It can be enabled even without extended_dynamic_state
    if (!Settings::values.vertex_input_dynamic_state.GetValue()) {
        RemoveExtensionFeature(extensions.vertex_input_dynamic_state, features.vertex_input_dynamic_state, VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME);
    }

    logical = vk::Device::Create(physical, queue_cis, ExtensionListForVulkan(loaded_extensions), first_next, dld);

    graphics_queue = logical.GetQueue(graphics_family);
    present_queue = logical.GetQueue(present_family);

    VmaVulkanFunctions functions{};
    functions.vkGetInstanceProcAddr = dld.vkGetInstanceProcAddr;
    functions.vkGetDeviceProcAddr = dld.vkGetDeviceProcAddr;

    VmaAllocatorCreateFlags flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT;
    if (extensions.memory_budget) {
        flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    }
    const VmaAllocatorCreateInfo allocator_info{
            .flags = flags,
            .physicalDevice = physical,
            .device = *logical,
            .preferredLargeHeapBlockSize = is_integrated
                                           ? (64u * 1024u * 1024u)
                                           : (256u * 1024u * 1024u),
            .pAllocationCallbacks = nullptr,
            .pDeviceMemoryCallbacks = nullptr,
            .pHeapSizeLimit = nullptr,
            .pVulkanFunctions = &functions,
            .instance = instance,
            .vulkanApiVersion = ApiVersion(),
            .pTypeExternalMemoryHandleTypes = nullptr,
    };

    vk::Check(vmaCreateAllocator(&allocator_info, &allocator));

    // Initialize GPU logging if enabled
    InitializeGPULogging();
}

Device::~Device() {
    ShutdownGPULogging();
    vmaDestroyAllocator(allocator);
}

VkFormat Device::GetSupportedFormat(VkFormat wanted_format, VkFormatFeatureFlags wanted_usage,
                                    FormatType format_type) const {
    if (IsFormatSupported(wanted_format, wanted_usage, format_type)) {
        return wanted_format;
    }
    // The wanted format is not supported by hardware, search for alternatives
    const VkFormat* alternatives = GetFormatAlternatives(wanted_format);
    if (alternatives == nullptr) {
        LOG_ERROR(Render_Vulkan,
                  "Format={} with usage={} and type={} has no defined alternatives and host "
                  "hardware does not support it",
                  wanted_format, wanted_usage, format_type);
        return wanted_format;
    }

    std::size_t i = 0;
    for (VkFormat alternative = *alternatives; alternative; alternative = alternatives[++i]) {
        if (!IsFormatSupported(alternative, wanted_usage, format_type)) {
            continue;
        }
        LOG_DEBUG(Render_Vulkan,
                  "Emulating format={} with alternative format={} with usage={} and type={}",
                  wanted_format, alternative, wanted_usage, format_type);
        return alternative;
    }

    // No alternatives found, panic
    LOG_ERROR(Render_Vulkan,
              "Format={} with usage={} and type={} is not supported by the host hardware and "
              "doesn't support any of the alternatives",
              wanted_format, wanted_usage, format_type);
    return wanted_format;
}

void Device::ReportLoss() const {
    LOG_CRITICAL(Render_Vulkan, "Device loss occurred!");

    // Wait for the log to flush and for Nsight Aftermath to dump the results
    std::this_thread::sleep_for(std::chrono::seconds{15});
}

void Device::SaveShader(std::span<const u32> spirv) const {
    if (nsight_aftermath_tracker) {
        nsight_aftermath_tracker->SaveShader(spirv);
    }
}

bool Device::ComputeIsOptimalAstcSupported() const {
    // Verify hardware supports all ASTC formats with optimal tiling to avoid software conversion
    static constexpr std::array<VkFormat, 28> astc_formats = {
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
    if (!features.features.textureCompressionASTC_LDR) {
        return false;
    }
    const auto format_feature_usage{VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
                                    VK_FORMAT_FEATURE_BLIT_SRC_BIT |
                                    VK_FORMAT_FEATURE_BLIT_DST_BIT |
                                    VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
                                    VK_FORMAT_FEATURE_TRANSFER_DST_BIT};
    for (const auto format : astc_formats) {
        const auto physical_format_properties{physical.GetFormatProperties(format)};
        if ((physical_format_properties.optimalTilingFeatures & format_feature_usage) == 0) {
            return false;
        }
    }
    return true;
}

bool Device::TestDepthStencilBlits(VkFormat format) const {
    static constexpr VkFormatFeatureFlags required_features =
        VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
    const auto test_features = [](VkFormatProperties props) {
        return (props.optimalTilingFeatures & required_features) == required_features;
    };
    return test_features(format_properties.at(format));
}

bool Device::IsFormatSupported(VkFormat wanted_format, VkFormatFeatureFlags wanted_usage,
                               FormatType format_type) const {
    const auto it = format_properties.find(wanted_format);
    if (it == format_properties.end()) {
        UNIMPLEMENTED_MSG("Unimplemented format query={}", wanted_format);
        return true;
    }
    const auto supported_usage = GetFormatFeatures(it->second, format_type);
    return (supported_usage & wanted_usage) == wanted_usage;
}

std::string Device::GetDriverName() const {
    return vk::GetDriverName(properties.driver);
}

bool Device::ShouldBoostClocks() const {
    const auto driver_id = properties.driver.driverID;
    const auto vendor_id = properties.properties.vendorID;
    const auto device_id = properties.properties.deviceID;

    const bool validated_driver =
        driver_id == VK_DRIVER_ID_AMD_PROPRIETARY || driver_id == VK_DRIVER_ID_AMD_OPEN_SOURCE ||
        driver_id == VK_DRIVER_ID_MESA_RADV || driver_id == VK_DRIVER_ID_NVIDIA_PROPRIETARY ||
        driver_id == VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS ||
        driver_id == VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA ||
        driver_id == VK_DRIVER_ID_QUALCOMM_PROPRIETARY || driver_id == VK_DRIVER_ID_MESA_TURNIP ||
        driver_id == VK_DRIVER_ID_SAMSUNG_PROPRIETARY;

    const bool is_steam_deck = (vendor_id == 0x1002 && device_id == 0x163F) ||
                               (vendor_id == 0x1002 && device_id == 0x1435);

    const bool is_debugging = this->HasDebuggingToolAttached();

    return validated_driver && !is_steam_deck && !is_debugging;
}

bool Device::HasTimelineSemaphore() const {
    if (GetDriverID() == VK_DRIVER_ID_MESA_TURNIP) {
        return false;
    }
    return features.timeline_semaphore.timelineSemaphore;
}

bool Device::GetSuitability(bool requires_swapchain) {
    // Assume we will be suitable.
    bool suitable = true;

    // Configure properties.
    VkPhysicalDeviceVulkan12Features features_1_2{};
    VkPhysicalDeviceVulkan13Features features_1_3{};

    // Configure properties.
    properties.properties = physical.GetProperties();

    // Set instance version.
    instance_version = properties.properties.apiVersion;

    // Minimum of API version 1.1 is required. (This is well-supported.)
    ASSERT(instance_version >= VK_API_VERSION_1_1);

    // Get available extensions.
    auto extension_properties = physical.EnumerateDeviceExtensionProperties();

    // Get the set of supported extensions.
    supported_extensions.clear();
    for (const VkExtensionProperties& property : extension_properties) {
        supported_extensions.insert(property.extensionName);
    }

    // Generate list of extensions to load.
    loaded_extensions.clear();

#define EXTENSION(prefix, macro_name, var_name)                                                    \
    if (supported_extensions.contains(VK_##prefix##_##macro_name##_EXTENSION_NAME)) {              \
            loaded_extensions.insert(VK_##prefix##_##macro_name##_EXTENSION_NAME);                     \
            extensions.var_name = true;                                                                \
    }
#define FEATURE_EXTENSION(prefix, struct_name, macro_name, var_name)                               \
    if (supported_extensions.contains(VK_##prefix##_##macro_name##_EXTENSION_NAME)) {              \
            loaded_extensions.insert(VK_##prefix##_##macro_name##_EXTENSION_NAME);                     \
            extensions.var_name = true;                                                                \
    }

    if (instance_version < VK_API_VERSION_1_2) {
        FOR_EACH_VK_FEATURE_1_2(FEATURE_EXTENSION);
    }
    if (instance_version < VK_API_VERSION_1_3) {
        FOR_EACH_VK_FEATURE_1_3(FEATURE_EXTENSION);
    }

    FOR_EACH_VK_FEATURE_EXT(FEATURE_EXTENSION);
    FOR_EACH_VK_EXTENSION(EXTENSION);

#undef FEATURE_EXTENSION
#undef EXTENSION

// Some extensions are mandatory. Check those.
#define CHECK_EXTENSION(extension_name)                                                            \
    if (!loaded_extensions.contains(extension_name)) {                                             \
            LOG_ERROR(Render_Vulkan, "Missing required extension {}", extension_name);                 \
            suitable = false;                                                                          \
    }

#define LOG_EXTENSION(extension_name)                                                              \
    if (!loaded_extensions.contains(extension_name)) {                                             \
            LOG_INFO(Render_Vulkan, "Device doesn't support extension {}", extension_name);            \
    }

    FOR_EACH_VK_RECOMMENDED_EXTENSION(LOG_EXTENSION);
    FOR_EACH_VK_MANDATORY_EXTENSION(CHECK_EXTENSION);

    if (requires_swapchain) {
        CHECK_EXTENSION(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

#undef LOG_EXTENSION
#undef CHECK_EXTENSION

    // Generate the linked list of features to test.
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

    // Set next pointer.
    void** next = &features2.pNext;

    // Vulkan 1.2 and 1.3 features
    if (instance_version >= VK_API_VERSION_1_2) {
        features_1_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        features_1_3.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

        features_1_2.pNext = &features_1_3;

        *next = &features_1_2;
    }

// Test all features we know about. If the feature is not available in core at our
// current API version, and was not enabled by an extension, skip testing the feature.
// We set the structure sType explicitly here as it is zeroed by the constructor.
#define FEATURE(prefix, struct_name, macro_name, var_name)                                         \
    features.var_name.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_##macro_name##_FEATURES;           \
        SetNext(next, features.var_name);

#define EXT_FEATURE(prefix, struct_name, macro_name, var_name)                                     \
    if (extensions.var_name) {                                                                     \
            features.var_name.sType =                                                                  \
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_##macro_name##_FEATURES_##prefix;                    \
            SetNext(next, features.var_name);                                                          \
    }

    FOR_EACH_VK_FEATURE_1_1(FEATURE);
    FOR_EACH_VK_FEATURE_EXT(EXT_FEATURE);
    if (instance_version >= VK_API_VERSION_1_2) {
        FOR_EACH_VK_FEATURE_1_2(FEATURE);
    } else {
        FOR_EACH_VK_FEATURE_1_2(EXT_FEATURE);
    }
    if (instance_version >= VK_API_VERSION_1_3) {
        FOR_EACH_VK_FEATURE_1_3(FEATURE);
    } else {
        FOR_EACH_VK_FEATURE_1_3(EXT_FEATURE);
    }

#undef EXT_FEATURE
#undef FEATURE

    // Perform the feature test.
    physical.GetFeatures2(features2);

    // Base Vulkan 1.0 features are always valid regardless of instance version.
    features.features = features2.features;

// Some features are mandatory. Check those.
#define CHECK_FEATURE(feature, name)                                                               \
    if (!features.feature.name) {                                                                  \
        if (IsMoltenVK() && (strcmp(#name, "geometryShader") == 0 ||                               \
                            strcmp(#name, "logicOp") == 0 ||                                       \
                            strcmp(#name, "shaderCullDistance") == 0 ||                            \
                            strcmp(#name, "wideLines") == 0)) {                                    \
            LOG_INFO(Render_Vulkan, "MoltenVK missing feature {} - using fallback", #name);       \
        } else {                                                                                    \
            LOG_ERROR(Render_Vulkan, "Missing required feature {}", #name);                        \
            suitable = false;                                                                       \
        }                                                                                           \
    }

#define LOG_FEATURE(feature, name)                                                                 \
    if (!features.feature.name) {                                                                  \
            LOG_INFO(Render_Vulkan, "Device doesn't support feature {}", #name);                       \
    }

// Optional features are enabled silently without any logging
#define OPTIONAL_FEATURE(feature, name) (void)features.feature.name;

    FOR_EACH_VK_OPTIONAL_FEATURE(OPTIONAL_FEATURE);
    FOR_EACH_VK_RECOMMENDED_FEATURE(LOG_FEATURE);
    FOR_EACH_VK_MANDATORY_FEATURE(CHECK_FEATURE);

#undef OPTIONAL_FEATURE
#undef LOG_FEATURE
#undef CHECK_FEATURE

    // Generate linked list of properties.
    properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

    // Set next pointer.
    next = &properties2.pNext;

    // Get driver info.
    properties.driver.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
    SetNext(next, properties.driver);

    // Retrieve subgroup properties.
    properties.subgroup_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
    SetNext(next, properties.subgroup_properties);

    // Retrieve relevant extension properties.
    if (extensions.shader_float_controls) {
        properties.float_controls.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES;
        SetNext(next, properties.float_controls);
    }
    if (extensions.push_descriptor) {
        properties.push_descriptor.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR;
        SetNext(next, properties.push_descriptor);
    }
    if (extensions.subgroup_size_control || features.subgroup_size_control.subgroupSizeControl) {
        properties.subgroup_size_control.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES;
        SetNext(next, properties.subgroup_size_control);
    }
    if (extensions.transform_feedback) {
        properties.transform_feedback.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT;
        SetNext(next, properties.transform_feedback);
    }
    if (extensions.maintenance5) {
        properties.maintenance5.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_PROPERTIES_KHR;
        SetNext(next, properties.maintenance5);
    }
    if (extensions.multi_draw) {
        properties.multi_draw.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTI_DRAW_PROPERTIES_EXT;
        SetNext(next, properties.multi_draw);
    }

    // Perform the property fetch.
    physical.GetProperties2(properties2);

    // Store base properties
    properties.properties = properties2.properties;

    // Unload extensions if feature support is insufficient.
    RemoveUnsuitableExtensions();

    // Check limits.
    struct Limit {
        u32 minimum;
        u32 value;
        const char* name;
    };

    const VkPhysicalDeviceLimits& limits{properties.properties.limits};
    const std::array limits_report{
                                   Limit{65536, limits.maxUniformBufferRange, "maxUniformBufferRange"},
                                   Limit{16, limits.maxViewports, "maxViewports"},
                                   Limit{8, limits.maxColorAttachments, "maxColorAttachments"},
                                   Limit{8, limits.maxClipDistances, "maxClipDistances"},
                                   };

    for (const auto& [min, value, name] : limits_report) {
        if (value < min) {
            LOG_ERROR(Render_Vulkan, "{} has to be {} or greater but it is {}", name, min, value);
            suitable = false;
        }
    }

    // VK_DYNAMIC_STATE

    // Driver detection variables for workarounds in GetSuitability
    const VkDriverId driver_id = properties.driver.driverID;

    // VK_EXT_extended_dynamic_state2 below this will appear drivers that need workarounds.

    // VK_EXT_extended_dynamic_state3 below this will appear drivers that need workarounds.

    // Samsung: Broken extendedDynamicState3ColorBlendEquation
    // Disable blend equation dynamic state, force static pipeline state
    if (extensions.extended_dynamic_state3 &&
        (driver_id == VK_DRIVER_ID_SAMSUNG_PROPRIETARY)) {
        LOG_WARNING(Render_Vulkan,
                    "Samsung: Disabling broken extendedDynamicState3ColorBlendEquation");
        features.extended_dynamic_state3.extendedDynamicState3ColorBlendEnable = false;
        features.extended_dynamic_state3.extendedDynamicState3ColorBlendEquation = false;
    }

    // Intel Windows < 27.20.100.0: Broken VertexInputDynamicState
    // Same for NVIDIA Proprietary < 580.119.02, unknown when VIDS was first NOT broken
    // Disable VertexInputDynamicState on old Intel Windows drivers
    if (extensions.vertex_input_dynamic_state) {
        const u32 version = (properties.properties.driverVersion << 3) >> 3;
        if ((driver_id == VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS && version < VK_MAKE_API_VERSION(27, 20, 100, 0))
        || (driver_id == VK_DRIVER_ID_NVIDIA_PROPRIETARY && version < VK_MAKE_API_VERSION(580, 119, 02, 0))) {
            LOG_WARNING(Render_Vulkan, "Disabling broken VK_EXT_vertex_input_dynamic_state");
            RemoveExtensionFeature(extensions.vertex_input_dynamic_state, features.vertex_input_dynamic_state, VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME);
        }
    }

    if (u32(Settings::values.dyna_state.GetValue()) == 0) {
        LOG_INFO(Render_Vulkan, "Extended Dynamic State disabled by user setting, clearing all EDS features");
        features.custom_border_color.customBorderColors = false;
        features.custom_border_color.customBorderColorWithoutFormat = false;
        features.extended_dynamic_state.extendedDynamicState = false;
        features.extended_dynamic_state2.extendedDynamicState2 = false;
        features.extended_dynamic_state3.extendedDynamicState3ColorBlendEnable = false;
        features.extended_dynamic_state3.extendedDynamicState3ColorBlendEquation = false;
        features.extended_dynamic_state3.extendedDynamicState3ColorWriteMask = false;
        features.extended_dynamic_state3.extendedDynamicState3DepthClampEnable = false;
        features.extended_dynamic_state3.extendedDynamicState3LogicOpEnable = false;
    }

    // Return whether we were suitable.
    return suitable;
}

void Device::RemoveUnsuitableExtensions() {
    // VK_EXT_custom_border_color
    // Enable extension if driver supports it, then check individual features
    // - customBorderColors: Required to use VK_BORDER_COLOR_FLOAT_CUSTOM_EXT
    // - customBorderColorWithoutFormat: Optional, allows VK_FORMAT_UNDEFINED
    // If only customBorderColors is available, we must provide a specific format
    if (extensions.custom_border_color) {
        // Verify that at least customBorderColors is available
        if (!features.custom_border_color.customBorderColors) {
            LOG_WARNING(Render_Vulkan,
                        "VK_EXT_custom_border_color reported but customBorderColors feature not available, disabling");
            extensions.custom_border_color = false;
        }
    }
    RemoveExtensionFeatureIfUnsuitable(extensions.custom_border_color, features.custom_border_color,
                                       VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME);
    // VK_KHR_unified_image_layouts
    extensions.unified_image_layouts = features.unified_image_layouts.unifiedImageLayouts;
    RemoveExtensionFeatureIfUnsuitable(extensions.unified_image_layouts, features.unified_image_layouts,
                                       VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION_NAME);

    // VK_EXT_depth_bias_control
    extensions.depth_bias_control =
        features.depth_bias_control.depthBiasControl &&
        features.depth_bias_control.leastRepresentableValueForceUnormRepresentation;
    RemoveExtensionFeatureIfUnsuitable(extensions.depth_bias_control, features.depth_bias_control,
                                       VK_EXT_DEPTH_BIAS_CONTROL_EXTENSION_NAME);

    // VK_EXT_depth_clip_control
    extensions.depth_clip_control = features.depth_clip_control.depthClipControl;
    RemoveExtensionFeatureIfUnsuitable(extensions.depth_clip_control, features.depth_clip_control,
                                       VK_EXT_DEPTH_CLIP_CONTROL_EXTENSION_NAME);

    /* */ // VK_EXT_extended_dynamic_state
    extensions.extended_dynamic_state = features.extended_dynamic_state.extendedDynamicState;
    RemoveExtensionFeatureIfUnsuitable(extensions.extended_dynamic_state,
                                       features.extended_dynamic_state,
                                       VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);

    // VK_EXT_extended_dynamic_state2
    extensions.extended_dynamic_state2 = features.extended_dynamic_state2.extendedDynamicState2;
    RemoveExtensionFeatureIfUnsuitable(extensions.extended_dynamic_state2,
                                       features.extended_dynamic_state2,
                                       VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME);

    // VK_EXT_extended_dynamic_state3
    const bool supports_color_blend_enable =
        features.extended_dynamic_state3.extendedDynamicState3ColorBlendEnable;
    const bool supports_color_blend_equation =
        features.extended_dynamic_state3.extendedDynamicState3ColorBlendEquation;
    const bool supports_color_write_mask =
        features.extended_dynamic_state3.extendedDynamicState3ColorWriteMask;
    dynamic_state3_blending = supports_color_blend_enable && supports_color_blend_equation &&
                              supports_color_write_mask;

    const bool supports_depth_clamp_enable =
        features.extended_dynamic_state3.extendedDynamicState3DepthClampEnable;
    const bool supports_logic_op_enable =
        features.extended_dynamic_state3.extendedDynamicState3LogicOpEnable;
    const bool supports_line_raster_mode =
        features.extended_dynamic_state3.extendedDynamicState3LineRasterizationMode &&
        extensions.line_rasterization && features.line_rasterization.rectangularLines;
    const bool supports_conservative_raster_mode =
        features.extended_dynamic_state3.extendedDynamicState3ConservativeRasterizationMode &&
        extensions.conservative_rasterization;
    const bool supports_line_stipple_enable =
        features.extended_dynamic_state3.extendedDynamicState3LineStippleEnable &&
        extensions.line_rasterization && features.line_rasterization.stippledRectangularLines;
    const bool supports_alpha_to_coverage =
        features.extended_dynamic_state3.extendedDynamicState3AlphaToCoverageEnable;
    const bool supports_alpha_to_one =
        features.extended_dynamic_state3.extendedDynamicState3AlphaToOneEnable &&
        features.features.alphaToOne;

    dynamic_state3_depth_clamp_enable = supports_depth_clamp_enable;
    dynamic_state3_logic_op_enable = supports_logic_op_enable;
    dynamic_state3_line_raster_mode = supports_line_raster_mode;
    dynamic_state3_conservative_raster_mode = supports_conservative_raster_mode;
    dynamic_state3_line_stipple_enable = supports_line_stipple_enable;
    dynamic_state3_alpha_to_coverage = supports_alpha_to_coverage;
    dynamic_state3_alpha_to_one = supports_alpha_to_one;

    dynamic_state3_enables = dynamic_state3_depth_clamp_enable || dynamic_state3_logic_op_enable ||
                             dynamic_state3_line_raster_mode ||
                             dynamic_state3_conservative_raster_mode ||
                             dynamic_state3_line_stipple_enable ||
                             dynamic_state3_alpha_to_coverage || dynamic_state3_alpha_to_one;

    extensions.extended_dynamic_state3 = dynamic_state3_blending || dynamic_state3_enables;
    if (!extensions.extended_dynamic_state3) {
        dynamic_state3_blending = false;
        dynamic_state3_enables = false;
        dynamic_state3_depth_clamp_enable = false;
        dynamic_state3_logic_op_enable = false;
        dynamic_state3_line_raster_mode = false;
        dynamic_state3_conservative_raster_mode = false;
        dynamic_state3_line_stipple_enable = false;
        dynamic_state3_alpha_to_coverage = false;
        dynamic_state3_alpha_to_one = false;
    }
    RemoveExtensionFeatureIfUnsuitable(extensions.extended_dynamic_state3,
                                       features.extended_dynamic_state3,
                                       VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME);

    // VK_EXT_robustness2
    // Enable if at least one robustness2 feature is available
    extensions.robustness_2 = features.robustness2.robustBufferAccess2 ||
                              features.robustness2.robustImageAccess2 ||
                              features.robustness2.nullDescriptor;

    RemoveExtensionFeatureIfUnsuitable(extensions.robustness_2, features.robustness2,
                                       VK_EXT_ROBUSTNESS_2_EXTENSION_NAME);

    // VK_EXT_image_robustness
    // Enable if robustImageAccess is available
    extensions.image_robustness = features.image_robustness.robustImageAccess;
    RemoveExtensionFeatureIfUnsuitable(extensions.image_robustness, features.image_robustness,
                                       VK_EXT_IMAGE_ROBUSTNESS_EXTENSION_NAME);

    // VK_EXT_provoking_vertex
    if (Settings::values.provoking_vertex.GetValue()) {
        extensions.provoking_vertex = features.provoking_vertex.provokingVertexLast
                                      && features.provoking_vertex
                                             .transformFeedbackPreservesProvokingVertex;
        RemoveExtensionFeatureIfUnsuitable(extensions.provoking_vertex,
                                           features.provoking_vertex,
                                           VK_EXT_PROVOKING_VERTEX_EXTENSION_NAME);
    } else {
        RemoveExtensionFeature(extensions.provoking_vertex,
                               features.provoking_vertex,
                               VK_EXT_PROVOKING_VERTEX_EXTENSION_NAME);
    }

    // VK_KHR_shader_atomic_int64
    extensions.shader_atomic_int64 = features.shader_atomic_int64.shaderBufferInt64Atomics &&
                                     features.shader_atomic_int64.shaderSharedInt64Atomics;
    RemoveExtensionFeatureIfUnsuitable(extensions.shader_atomic_int64, features.shader_atomic_int64,
                                       VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME);

    // VK_EXT_shader_demote_to_helper_invocation
    extensions.shader_demote_to_helper_invocation =
        features.shader_demote_to_helper_invocation.shaderDemoteToHelperInvocation;
    RemoveExtensionFeatureIfUnsuitable(extensions.shader_demote_to_helper_invocation,
                                       features.shader_demote_to_helper_invocation,
                                       VK_EXT_SHADER_DEMOTE_TO_HELPER_INVOCATION_EXTENSION_NAME);

    // VK_EXT_subgroup_size_control
    extensions.subgroup_size_control =
        features.subgroup_size_control.subgroupSizeControl &&
        properties.subgroup_size_control.minSubgroupSize <= GuestWarpSize &&
        properties.subgroup_size_control.maxSubgroupSize >= GuestWarpSize;
    RemoveExtensionFeatureIfUnsuitable(extensions.subgroup_size_control,
                                       features.subgroup_size_control,
                                       VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME);

    // VK_EXT_transform_feedback
    // We only require the basic transformFeedback feature and at least
    // one transform feedback buffer. We keep transformFeedbackQueries as it's used by
    // the streaming byte count implementation. GeometryStreams and multiple streams
    // are not strictly required since we currently support only stream 0.
    extensions.transform_feedback =
        features.transform_feedback.transformFeedback &&
        properties.transform_feedback.maxTransformFeedbackBuffers > 0 &&
        properties.transform_feedback.transformFeedbackQueries;
    RemoveExtensionFeatureIfUnsuitable(extensions.transform_feedback, features.transform_feedback,
                                       VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME);
    if (extensions.transform_feedback) {
        LOG_INFO(Render_Vulkan, "VK_EXT_transform_feedback enabled (buffers={}, queries={})",
                 properties.transform_feedback.maxTransformFeedbackBuffers,
                 properties.transform_feedback.transformFeedbackQueries);
    }

    // VK_EXT_vertex_input_dynamic_state
    extensions.vertex_input_dynamic_state =
        features.vertex_input_dynamic_state.vertexInputDynamicState;
    RemoveExtensionFeatureIfUnsuitable(extensions.vertex_input_dynamic_state,
                                       features.vertex_input_dynamic_state,
                                       VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME);

    // VK_EXT_multi_draw
    extensions.multi_draw = features.multi_draw.multiDraw;

    if (extensions.multi_draw) {
        LOG_INFO(Render_Vulkan, "VK_EXT_multi_draw: maxMultiDrawCount={}",
                 properties.multi_draw.maxMultiDrawCount);
    }

    RemoveExtensionFeatureIfUnsuitable(extensions.multi_draw, features.multi_draw,
                                       VK_EXT_MULTI_DRAW_EXTENSION_NAME);

    // VK_KHR_pipeline_executable_properties
    if (Settings::values.renderer_shader_feedback.GetValue()) {
        extensions.pipeline_executable_properties =
            features.pipeline_executable_properties.pipelineExecutableInfo;
        RemoveExtensionFeatureIfUnsuitable(extensions.pipeline_executable_properties,
                                           features.pipeline_executable_properties,
                                           VK_KHR_PIPELINE_EXECUTABLE_PROPERTIES_EXTENSION_NAME);
    } else {
        RemoveExtensionFeature(extensions.pipeline_executable_properties,
                               features.pipeline_executable_properties,
                               VK_KHR_PIPELINE_EXECUTABLE_PROPERTIES_EXTENSION_NAME);
    }

    // VK_KHR_workgroup_memory_explicit_layout
    extensions.workgroup_memory_explicit_layout =
        features.features.shaderInt16 &&
        features.workgroup_memory_explicit_layout.workgroupMemoryExplicitLayout &&
        features.workgroup_memory_explicit_layout.workgroupMemoryExplicitLayout8BitAccess &&
        features.workgroup_memory_explicit_layout.workgroupMemoryExplicitLayout16BitAccess &&
        features.workgroup_memory_explicit_layout.workgroupMemoryExplicitLayoutScalarBlockLayout;
    RemoveExtensionFeatureIfUnsuitable(extensions.workgroup_memory_explicit_layout,
                                       features.workgroup_memory_explicit_layout,
                                       VK_KHR_WORKGROUP_MEMORY_EXPLICIT_LAYOUT_EXTENSION_NAME);

    // VK_EXT_swapchain_maintenance1 (extension only, has features)
    // Requires VK_EXT_surface_maintenance1 instance extension
    extensions.swapchain_maintenance1 = features.swapchain_maintenance1.swapchainMaintenance1;
    if (extensions.swapchain_maintenance1) {
        // Check if VK_EXT_surface_maintenance1 instance extension is available
        const auto instance_extensions = vk::EnumerateInstanceExtensionProperties(dld);
        const bool has_surface_maintenance1 = instance_extensions && std::ranges::any_of(*instance_extensions,
            [](const VkExtensionProperties& prop) {
                return std::strcmp(prop.extensionName, VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME) == 0;
            });
        if (!has_surface_maintenance1) {
            LOG_WARNING(Render_Vulkan,
                        "VK_EXT_swapchain_maintenance1 requires VK_EXT_surface_maintenance1, disabling");
            extensions.swapchain_maintenance1 = false;
            features.swapchain_maintenance1.swapchainMaintenance1 = false;
        }
    }
    RemoveExtensionFeatureIfUnsuitable(extensions.swapchain_maintenance1, features.swapchain_maintenance1,
                                       VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);

    // VK_KHR_maintenance1 (core in Vulkan 1.1, no features)
    extensions.maintenance1 = loaded_extensions.contains(VK_KHR_MAINTENANCE_1_EXTENSION_NAME);
    RemoveExtensionIfUnsuitable(extensions.maintenance1, VK_KHR_MAINTENANCE_1_EXTENSION_NAME);

    // VK_KHR_maintenance2 (core in Vulkan 1.1, no features)
    extensions.maintenance2 = loaded_extensions.contains(VK_KHR_MAINTENANCE_2_EXTENSION_NAME);
    RemoveExtensionIfUnsuitable(extensions.maintenance2, VK_KHR_MAINTENANCE_2_EXTENSION_NAME);

    // VK_KHR_maintenance3 (core in Vulkan 1.1, no features)
    extensions.maintenance3 = loaded_extensions.contains(VK_KHR_MAINTENANCE_3_EXTENSION_NAME);
    RemoveExtensionIfUnsuitable(extensions.maintenance3, VK_KHR_MAINTENANCE_3_EXTENSION_NAME);

    // VK_KHR_maintenance4
    extensions.maintenance4 = features.maintenance4.maintenance4;
    RemoveExtensionFeatureIfUnsuitable(extensions.maintenance4, features.maintenance4,
                                       VK_KHR_MAINTENANCE_4_EXTENSION_NAME);

    // VK_KHR_maintenance5
    extensions.maintenance5 = features.maintenance5.maintenance5;

    if (extensions.maintenance5) {
        LOG_INFO(Render_Vulkan, "VK_KHR_maintenance5 properties: polygonModePointSize={} "
                                "depthStencilSwizzleOne={} earlyFragmentTests={} nonStrictWideLines={}",
                 properties.maintenance5.polygonModePointSize,
                 properties.maintenance5.depthStencilSwizzleOneSupport,
                 properties.maintenance5.earlyFragmentMultisampleCoverageAfterSampleCounting &&
                 properties.maintenance5.earlyFragmentSampleMaskTestBeforeSampleCounting,
                 properties.maintenance5.nonStrictWideLinesUseParallelogram);
    }

    RemoveExtensionFeatureIfUnsuitable(extensions.maintenance5, features.maintenance5,
                                       VK_KHR_MAINTENANCE_5_EXTENSION_NAME);

    // VK_KHR_maintenance6
    extensions.maintenance6 = features.maintenance6.maintenance6;
    RemoveExtensionFeatureIfUnsuitable(extensions.maintenance6, features.maintenance6,
                                       VK_KHR_MAINTENANCE_6_EXTENSION_NAME);

    // VK_KHR_maintenance7 (proposed for Vulkan 1.4, no features)
    extensions.maintenance7 = loaded_extensions.contains(VK_KHR_MAINTENANCE_7_EXTENSION_NAME);
    RemoveExtensionIfUnsuitable(extensions.maintenance7, VK_KHR_MAINTENANCE_7_EXTENSION_NAME);

    // VK_KHR_maintenance8 (proposed for Vulkan 1.4, no features)
    extensions.maintenance8 = loaded_extensions.contains(VK_KHR_MAINTENANCE_8_EXTENSION_NAME);
    RemoveExtensionIfUnsuitable(extensions.maintenance8, VK_KHR_MAINTENANCE_8_EXTENSION_NAME);

    // VK_KHR_maintenance9 (proposed for Vulkan 1.4, no features)
    extensions.maintenance9 = loaded_extensions.contains(VK_KHR_MAINTENANCE_9_EXTENSION_NAME);
    RemoveExtensionIfUnsuitable(extensions.maintenance9, VK_KHR_MAINTENANCE_9_EXTENSION_NAME);
}

void Device::SetupFamilies(VkSurfaceKHR surface) {
    const std::vector queue_family_properties = physical.GetQueueFamilyProperties();
    std::optional<u32> graphics;
    std::optional<u32> present;
    for (u32 index = 0; index < static_cast<u32>(queue_family_properties.size()); ++index) {
        if (graphics && (present || !surface)) {
            break;
        }
        const VkQueueFamilyProperties& queue_family = queue_family_properties[index];
        if (queue_family.queueCount == 0) {
            continue;
        }
        if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics = index;
        }
        if (surface && physical.GetSurfaceSupportKHR(index, surface)) {
            present = index;
        }
    }
    if (!graphics) {
        LOG_ERROR(Render_Vulkan, "Device lacks a graphics queue");
        throw vk::Exception(VK_ERROR_FEATURE_NOT_PRESENT);
    }
    if (surface && !present) {
        LOG_ERROR(Render_Vulkan, "Device lacks a present queue");
        throw vk::Exception(VK_ERROR_FEATURE_NOT_PRESENT);
    }
    if (graphics) {
        graphics_family = *graphics;
    }
    if (present) {
        present_family = *present;
    }
}

std::optional<size_t> Device::GetSamplerHeapBudget() const {
    if (sampler_heap_budget == 0) {
        return std::nullopt;
    }
    return sampler_heap_budget;
}

u64 Device::GetDeviceMemoryUsage() const {
    VkPhysicalDeviceMemoryBudgetPropertiesEXT budget;
    budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
    budget.pNext = nullptr;
    physical.GetMemoryProperties(&budget);
    u64 result{};
    for (const size_t heap : valid_heap_memory) {
        result += budget.heapUsage[heap];
    }
    return result;
}

void Device::CollectPhysicalMemoryInfo() {
    // Calculate limits using memory budget
    VkPhysicalDeviceMemoryBudgetPropertiesEXT budget{};
    budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
    const auto mem_info =
        physical.GetMemoryProperties(extensions.memory_budget ? &budget : nullptr);
    const auto& mem_properties = mem_info.memoryProperties;
    const size_t num_properties = mem_properties.memoryHeapCount;
    device_access_memory = 0;
    u64 device_initial_usage = 0;
    u64 local_memory = 0;
    for (size_t element = 0; element < num_properties; ++element) {
        const bool is_heap_local =
            (mem_properties.memoryHeaps[element].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0;
        if (!is_integrated && !is_heap_local) {
            continue;
        }
        valid_heap_memory.push_back(element);
        if (is_heap_local) {
            local_memory += mem_properties.memoryHeaps[element].size;
        }
        if (extensions.memory_budget) {
            device_initial_usage += budget.heapUsage[element];
            device_access_memory += budget.heapBudget[element];
            continue;
        }
        device_access_memory += mem_properties.memoryHeaps[element].size;
    }
    if (is_integrated) {
        const s64 available_memory = static_cast<s64>(device_access_memory - device_initial_usage);
        const u64 memory_size = Settings::values.vram_usage_mode.GetValue() == Settings::VramUsageMode::Aggressive ? 6_GiB : 4_GiB;
        device_access_memory = static_cast<u64>(std::max<s64>(std::min<s64>(available_memory - 8_GiB, memory_size), std::min<s64>(local_memory, memory_size)));
    } else {
        const u64 reserve_memory = std::min<u64>(device_access_memory / 8, 1_GiB);
        device_access_memory -= reserve_memory;
        if (Settings::values.vram_usage_mode.GetValue() != Settings::VramUsageMode::Aggressive) {
            // Account for resolution scaling in memory limits
            const size_t normal_memory = 6_GiB;
            const size_t scaler_memory = 1_GiB * Settings::values.resolution_info.ScaleUp(1);
            device_access_memory = std::min<u64>(device_access_memory, normal_memory + scaler_memory);
        }
    }
}

void Device::CollectToolingInfo() {
    if (!extensions.tooling_info) {
        return;
    }
    auto tools{physical.GetPhysicalDeviceToolProperties()};
    for (const VkPhysicalDeviceToolProperties& tool : tools) {
        const std::string_view name = tool.name;
        LOG_INFO(Render_Vulkan, "Attached debugging tool: {}", name);
        has_renderdoc = has_renderdoc || name == "RenderDoc";
        has_nsight_graphics = has_nsight_graphics || name == "NVIDIA Nsight Graphics";
        has_radeon_gpu_profiler = has_radeon_gpu_profiler || name == "Radeon GPU Profiler";
    }
}

std::vector<VkDeviceQueueCreateInfo> Device::GetDeviceQueueCreateInfos() const {
    static constexpr float QUEUE_PRIORITY = 1.0f;

    ankerl::unordered_dense::set<u32> unique_queue_families{graphics_family, present_family};
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

void Device::InitializeGPULogging() {
    if (!Settings::values.gpu_logging_enabled.GetValue()) {
        return;
    }

    // Detect driver type
    const auto driver_id = GetDriverID();
    GPU::Logging::DriverType detected_driver = GPU::Logging::DriverType::Unknown;

    if (driver_id == VK_DRIVER_ID_MESA_TURNIP) {
        detected_driver = GPU::Logging::DriverType::Turnip;
    } else if (driver_id == VK_DRIVER_ID_QUALCOMM_PROPRIETARY) {
        detected_driver = GPU::Logging::DriverType::Qualcomm;
    }

    // Get log level from settings
    const auto log_level = static_cast<GPU::Logging::LogLevel>(
        static_cast<u32>(Settings::values.gpu_log_level.GetValue()));

    // Initialize GPU logger
    GPU::Logging::GPULogger::GetInstance().Initialize(log_level, detected_driver);

    // Configure feature flags
    GPU::Logging::GPULogger::GetInstance().EnableVulkanCallTracking(
        Settings::values.gpu_log_vulkan_calls.GetValue());
    GPU::Logging::GPULogger::GetInstance().EnableShaderDumps(
        Settings::values.gpu_log_shader_dumps.GetValue());
    GPU::Logging::GPULogger::GetInstance().EnableMemoryTracking(
        Settings::values.gpu_log_memory_tracking.GetValue());
    GPU::Logging::GPULogger::GetInstance().EnableDriverDebugInfo(
        Settings::values.gpu_log_driver_debug.GetValue());
    GPU::Logging::GPULogger::GetInstance().SetRingBufferSize(
        Settings::values.gpu_log_ring_buffer_size.GetValue());

    // Log comprehensive driver and extension information
    if (Settings::values.gpu_log_driver_debug.GetValue()) {
        std::string driver_info;

        // Device information
        const auto& props = properties.properties;
        driver_info += fmt::format("Device: {}\n", props.deviceName);
        driver_info += fmt::format("Driver Name: {}\n", properties.driver.driverName);
        driver_info += fmt::format("Driver Info: {}\n", properties.driver.driverInfo);

        // Version information
        const u32 driver_version = props.driverVersion;
        const u32 api_version = props.apiVersion;
        driver_info += fmt::format("Driver Version: {}.{}.{}\n",
            VK_API_VERSION_MAJOR(driver_version),
            VK_API_VERSION_MINOR(driver_version),
            VK_API_VERSION_PATCH(driver_version));
        driver_info += fmt::format("Vulkan API Version: {}.{}.{}\n",
            VK_API_VERSION_MAJOR(api_version),
            VK_API_VERSION_MINOR(api_version),
            VK_API_VERSION_PATCH(api_version));
        driver_info += fmt::format("Driver ID: {}\n", static_cast<u32>(driver_id));

        // Vendor and device IDs
        driver_info += fmt::format("Vendor ID: 0x{:04X}\n", props.vendorID);
        driver_info += fmt::format("Device ID: 0x{:04X}\n", props.deviceID);

        // Extensions - separate QCOM extensions from others
        driver_info += "\n=== Loaded Vulkan Extensions ===\n";
        std::vector<std::string> qcom_exts;
        std::vector<std::string> other_exts;

        for (const auto& ext : loaded_extensions) {
            if (ext.find("QCOM") != std::string::npos || ext.find("qcom") != std::string::npos) {
                qcom_exts.push_back(ext);
            } else {
                other_exts.push_back(ext);
            }
        }

        // Log QCOM extensions first
        if (!qcom_exts.empty()) {
            driver_info += "\nQualcomm Proprietary Extensions:\n";
            for (const auto& ext : qcom_exts) {
                driver_info += fmt::format("  - {}\n", ext);
            }
        }

        // Log other extensions
        if (!other_exts.empty()) {
            driver_info += "\nStandard Extensions:\n";
            for (const auto& ext : other_exts) {
                driver_info += fmt::format("  - {}\n", ext);
            }
        }

        driver_info += fmt::format("\nTotal Extensions Loaded: {}\n", loaded_extensions.size());

        GPU::Logging::GPULogger::GetInstance().LogDriverDebugInfo(driver_info);
    }
}

void Device::ShutdownGPULogging() {
    if (GPU::Logging::GPULogger::GetInstance().IsInitialized()) {
        GPU::Logging::GPULogger::GetInstance().Shutdown();
    }
}

} // namespace Vulkan
