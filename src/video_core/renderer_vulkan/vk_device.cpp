// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <map>
#include <optional>
#include <set>
#include <vector>
#include "common/assert.h"
#include "video_core/renderer_vulkan/declarations.h"
#include "video_core/renderer_vulkan/vk_device.h"

namespace Vulkan {

namespace Alternatives {

constexpr std::array<vk::Format, 3> Depth24UnormS8Uint = {
    vk::Format::eD32SfloatS8Uint, vk::Format::eD16UnormS8Uint, {}};
constexpr std::array<vk::Format, 3> Depth16UnormS8Uint = {
    vk::Format::eD24UnormS8Uint, vk::Format::eD32SfloatS8Uint, {}};
constexpr std::array<vk::Format, 2> Astc = {vk::Format::eA8B8G8R8UnormPack32, {}};

} // namespace Alternatives

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

constexpr vk::FormatFeatureFlags GetFormatFeatures(vk::FormatProperties properties,
                                                   FormatType format_type) {
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

VKDevice::VKDevice(const vk::DispatchLoaderDynamic& dldi, vk::PhysicalDevice physical,
                   vk::SurfaceKHR surface)
    : physical{physical}, format_properties{GetFormatProperties(dldi, physical)} {
    SetupFamilies(dldi, surface);
    SetupProperties(dldi);
    SetupFeatures(dldi);
}

VKDevice::~VKDevice() = default;

bool VKDevice::Create(const vk::DispatchLoaderDynamic& dldi, vk::Instance instance) {
    vk::PhysicalDeviceFeatures device_features;
    device_features.vertexPipelineStoresAndAtomics = true;
    device_features.independentBlend = true;
    device_features.textureCompressionASTC_LDR = is_optimal_astc_supported;

    const auto queue_cis = GetDeviceQueueCreateInfos();
    const std::vector<const char*> extensions = LoadExtensions(dldi);
    const vk::DeviceCreateInfo device_ci({}, static_cast<u32>(queue_cis.size()), queue_cis.data(),
                                         0, nullptr, static_cast<u32>(extensions.size()),
                                         extensions.data(), &device_features);
    vk::Device dummy_logical;
    if (physical.createDevice(&device_ci, nullptr, &dummy_logical, dldi) != vk::Result::eSuccess) {
        LOG_CRITICAL(Render_Vulkan, "Logical device failed to be created!");
        return false;
    }

    dld.init(instance, dldi.vkGetInstanceProcAddr, dummy_logical, dldi.vkGetDeviceProcAddr);
    logical = UniqueDevice(
        dummy_logical, vk::ObjectDestroy<vk::NoParent, vk::DispatchLoaderDynamic>(nullptr, dld));

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
        LOG_CRITICAL(Render_Vulkan,
                     "Format={} with usage={} and type={} has no defined alternatives and host "
                     "hardware does not support it",
                     vk::to_string(wanted_format), vk::to_string(wanted_usage),
                     static_cast<u32>(format_type));
        UNREACHABLE();
        return wanted_format;
    }

    std::size_t i = 0;
    for (vk::Format alternative = alternatives[0]; alternative != vk::Format{};
         alternative = alternatives[++i]) {
        if (!IsFormatSupported(alternative, wanted_usage, format_type))
            continue;
        LOG_WARNING(Render_Vulkan,
                    "Emulating format={} with alternative format={} with usage={} and type={}",
                    static_cast<u32>(wanted_format), static_cast<u32>(alternative),
                    static_cast<u32>(wanted_usage), static_cast<u32>(format_type));
        return alternative;
    }

    // No alternatives found, panic
    LOG_CRITICAL(Render_Vulkan,
                 "Format={} with usage={} and type={} is not supported by the host hardware and "
                 "doesn't support any of the alternatives",
                 static_cast<u32>(wanted_format), static_cast<u32>(wanted_usage),
                 static_cast<u32>(format_type));
    UNREACHABLE();
    return wanted_format;
}

bool VKDevice::IsOptimalAstcSupported(const vk::PhysicalDeviceFeatures& features,
                                      const vk::DispatchLoaderDynamic& dldi) const {
    if (!features.textureCompressionASTC_LDR) {
        return false;
    }
    const auto format_feature_usage{
        vk::FormatFeatureFlagBits::eSampledImage | vk::FormatFeatureFlagBits::eBlitSrc |
        vk::FormatFeatureFlagBits::eBlitDst | vk::FormatFeatureFlagBits::eTransferSrc |
        vk::FormatFeatureFlagBits::eTransferDst};
    constexpr std::array<vk::Format, 9> astc_formats = {
        vk::Format::eAstc4x4UnormBlock, vk::Format::eAstc4x4SrgbBlock,
        vk::Format::eAstc8x8SrgbBlock,  vk::Format::eAstc8x6SrgbBlock,
        vk::Format::eAstc5x4SrgbBlock,  vk::Format::eAstc5x5UnormBlock,
        vk::Format::eAstc5x5SrgbBlock,  vk::Format::eAstc10x8UnormBlock,
        vk::Format::eAstc10x8SrgbBlock};
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
        LOG_CRITICAL(Render_Vulkan, "Unimplemented format query={}", vk::to_string(wanted_format));
        UNREACHABLE();
        return true;
    }
    const vk::FormatFeatureFlags supported_usage = GetFormatFeatures(it->second, format_type);
    return (supported_usage & wanted_usage) == wanted_usage;
}

bool VKDevice::IsSuitable(const vk::DispatchLoaderDynamic& dldi, vk::PhysicalDevice physical,
                          vk::SurfaceKHR surface) {
    bool has_swapchain{};
    for (const auto& prop : physical.enumerateDeviceExtensionProperties(nullptr, dldi)) {
        has_swapchain |= prop.extensionName == std::string(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }
    if (!has_swapchain) {
        // The device doesn't support creating swapchains.
        return false;
    }

    bool has_graphics{}, has_present{};
    const auto queue_family_properties = physical.getQueueFamilyProperties(dldi);
    for (u32 i = 0; i < static_cast<u32>(queue_family_properties.size()); ++i) {
        const auto& family = queue_family_properties[i];
        if (family.queueCount == 0)
            continue;

        has_graphics |=
            (family.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlagBits>(0);
        has_present |= physical.getSurfaceSupportKHR(i, surface, dldi) != 0;
    }
    if (!has_graphics || !has_present) {
        // The device doesn't have a graphics and present queue.
        return false;
    }

    // TODO(Rodrigo): Check if the device matches all requeriments.
    const auto properties{physical.getProperties(dldi)};
    const auto limits{properties.limits};
    if (limits.maxUniformBufferRange < 65536) {
        return false;
    }

    const vk::PhysicalDeviceFeatures features{physical.getFeatures(dldi)};
    if (!features.vertexPipelineStoresAndAtomics || !features.independentBlend) {
        return false;
    }

    // Device is suitable.
    return true;
}

std::vector<const char*> VKDevice::LoadExtensions(const vk::DispatchLoaderDynamic& dldi) {
    std::vector<const char*> extensions;
    extensions.reserve(2);
    extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    const auto Test = [&](const vk::ExtensionProperties& extension,
                          std::optional<std::reference_wrapper<bool>> status, const char* name,
                          u32 revision) {
        if (extension.extensionName != std::string(name)) {
            return;
        }
        extensions.push_back(name);
        if (status) {
            status->get() = true;
        }
    };

    for (const auto& extension : physical.enumerateDeviceExtensionProperties(nullptr, dldi)) {
        Test(extension, ext_scalar_block_layout, VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME, 1);
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

void VKDevice::SetupProperties(const vk::DispatchLoaderDynamic& dldi) {
    const vk::PhysicalDeviceProperties props = physical.getProperties(dldi);
    device_type = props.deviceType;
    uniform_buffer_alignment = static_cast<u64>(props.limits.minUniformBufferOffsetAlignment);
    max_storage_buffer_range = static_cast<u64>(props.limits.maxStorageBufferRange);
}

void VKDevice::SetupFeatures(const vk::DispatchLoaderDynamic& dldi) {
    const auto supported_features{physical.getFeatures(dldi)};
    is_optimal_astc_supported = IsOptimalAstcSupported(supported_features, dldi);
}

std::vector<vk::DeviceQueueCreateInfo> VKDevice::GetDeviceQueueCreateInfos() const {
    static const float QUEUE_PRIORITY = 1.0f;

    std::set<u32> unique_queue_families = {graphics_family, present_family};
    std::vector<vk::DeviceQueueCreateInfo> queue_cis;

    for (u32 queue_family : unique_queue_families)
        queue_cis.push_back({{}, queue_family, 1, &QUEUE_PRIORITY});

    return queue_cis;
}

std::map<vk::Format, vk::FormatProperties> VKDevice::GetFormatProperties(
    const vk::DispatchLoaderDynamic& dldi, vk::PhysicalDevice physical) {
    static constexpr std::array formats{vk::Format::eA8B8G8R8UnormPack32,
                                        vk::Format::eB5G6R5UnormPack16,
                                        vk::Format::eA2B10G10R10UnormPack32,
                                        vk::Format::eR32G32B32A32Sfloat,
                                        vk::Format::eR16G16Unorm,
                                        vk::Format::eR16G16Snorm,
                                        vk::Format::eR8G8B8A8Srgb,
                                        vk::Format::eR8Unorm,
                                        vk::Format::eB10G11R11UfloatPack32,
                                        vk::Format::eR32Sfloat,
                                        vk::Format::eR16Sfloat,
                                        vk::Format::eR16G16B16A16Sfloat,
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
                                        vk::Format::eAstc4x4UnormBlock,
                                        vk::Format::eAstc4x4SrgbBlock,
                                        vk::Format::eAstc8x8SrgbBlock,
                                        vk::Format::eAstc8x6SrgbBlock,
                                        vk::Format::eAstc5x4SrgbBlock,
                                        vk::Format::eAstc5x5UnormBlock,
                                        vk::Format::eAstc5x5SrgbBlock,
                                        vk::Format::eAstc10x8UnormBlock,
                                        vk::Format::eAstc10x8SrgbBlock};
    std::map<vk::Format, vk::FormatProperties> format_properties;
    for (const auto format : formats) {
        format_properties.emplace(format, physical.getFormatProperties(format, dldi));
    }
    return format_properties;
}

} // namespace Vulkan
