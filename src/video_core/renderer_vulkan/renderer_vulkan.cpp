// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <fmt/format.h>

#include "common/dynamic_library.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/telemetry.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/frontend/emu_window.h"
#include "core/settings.h"
#include "core/telemetry_session.h"
#include "video_core/gpu.h"
#include "video_core/renderer_vulkan/renderer_vulkan.h"
#include "video_core/renderer_vulkan/vk_blit_screen.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_memory_manager.h"
#include "video_core/renderer_vulkan/vk_rasterizer.h"
#include "video_core/renderer_vulkan/vk_resource_manager.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_state_tracker.h"
#include "video_core/renderer_vulkan/vk_swapchain.h"
#include "video_core/renderer_vulkan/wrapper.h"

// Include these late to avoid polluting previous headers
#ifdef _WIN32
#include <windows.h>
// ensure include order
#include <vulkan/vulkan_win32.h>
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
#include <X11/Xlib.h>
#include <vulkan/vulkan_wayland.h>
#include <vulkan/vulkan_xlib.h>
#endif

namespace Vulkan {

namespace {

using Core::Frontend::WindowSystemType;

VkBool32 DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                       VkDebugUtilsMessageTypeFlagsEXT type,
                       const VkDebugUtilsMessengerCallbackDataEXT* data,
                       [[maybe_unused]] void* user_data) {
    const char* message{data->pMessage};

    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        LOG_CRITICAL(Render_Vulkan, "{}", message);
    } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        LOG_WARNING(Render_Vulkan, "{}", message);
    } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        LOG_INFO(Render_Vulkan, "{}", message);
    } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        LOG_DEBUG(Render_Vulkan, "{}", message);
    }
    return VK_FALSE;
}

Common::DynamicLibrary OpenVulkanLibrary() {
    Common::DynamicLibrary library;
#ifdef __APPLE__
    // Check if a path to a specific Vulkan library has been specified.
    char* libvulkan_env = getenv("LIBVULKAN_PATH");
    if (!libvulkan_env || !library.Open(libvulkan_env)) {
        // Use the libvulkan.dylib from the application bundle.
        const std::string filename =
            FileUtil::GetBundleDirectory() + "/Contents/Frameworks/libvulkan.dylib";
        library.Open(filename.c_str());
    }
#else
    std::string filename = Common::DynamicLibrary::GetVersionedFilename("vulkan", 1);
    if (!library.Open(filename.c_str())) {
        // Android devices may not have libvulkan.so.1, only libvulkan.so.
        filename = Common::DynamicLibrary::GetVersionedFilename("vulkan");
        library.Open(filename.c_str());
    }
#endif
    return library;
}

vk::Instance CreateInstance(Common::DynamicLibrary& library, vk::InstanceDispatch& dld,
                            WindowSystemType window_type = WindowSystemType::Headless,
                            bool enable_layers = false) {
    if (!library.IsOpen()) {
        LOG_ERROR(Render_Vulkan, "Vulkan library not available");
        return {};
    }
    if (!library.GetSymbol("vkGetInstanceProcAddr", &dld.vkGetInstanceProcAddr)) {
        LOG_ERROR(Render_Vulkan, "vkGetInstanceProcAddr not present in Vulkan");
        return {};
    }
    if (!vk::Load(dld)) {
        LOG_ERROR(Render_Vulkan, "Failed to load Vulkan function pointers");
        return {};
    }

    std::vector<const char*> extensions;
    extensions.reserve(6);
    switch (window_type) {
    case Core::Frontend::WindowSystemType::Headless:
        break;
#ifdef _WIN32
    case Core::Frontend::WindowSystemType::Windows:
        extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
        break;
#endif
#if !defined(_WIN32) && !defined(__APPLE__)
    case Core::Frontend::WindowSystemType::X11:
        extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
        break;
    case Core::Frontend::WindowSystemType::Wayland:
        extensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
        break;
#endif
    default:
        LOG_ERROR(Render_Vulkan, "Presentation not supported on this platform");
        break;
    }
    if (window_type != Core::Frontend::WindowSystemType::Headless) {
        extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    }
    if (enable_layers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    const std::optional properties = vk::EnumerateInstanceExtensionProperties(dld);
    if (!properties) {
        LOG_ERROR(Render_Vulkan, "Failed to query extension properties");
        return {};
    }

    for (const char* extension : extensions) {
        const auto it =
            std::find_if(properties->begin(), properties->end(), [extension](const auto& prop) {
                return !std::strcmp(extension, prop.extensionName);
            });
        if (it == properties->end()) {
            LOG_ERROR(Render_Vulkan, "Required instance extension {} is not available", extension);
            return {};
        }
    }

    static constexpr std::array layers_data{"VK_LAYER_LUNARG_standard_validation"};
    vk::Span<const char*> layers = layers_data;
    if (!enable_layers) {
        layers = {};
    }
    vk::Instance instance = vk::Instance::Create(layers, extensions, dld);
    if (!instance) {
        LOG_ERROR(Render_Vulkan, "Failed to create Vulkan instance");
        return {};
    }
    if (!vk::Load(*instance, dld)) {
        LOG_ERROR(Render_Vulkan, "Failed to load Vulkan instance function pointers");
    }
    return instance;
}

std::string GetReadableVersion(u32 version) {
    return fmt::format("{}.{}.{}", VK_VERSION_MAJOR(version), VK_VERSION_MINOR(version),
                       VK_VERSION_PATCH(version));
}

std::string GetDriverVersion(const VKDevice& device) {
    // Extracted from
    // https://github.com/SaschaWillems/vulkan.gpuinfo.org/blob/5dddea46ea1120b0df14eef8f15ff8e318e35462/functions.php#L308-L314
    const u32 version = device.GetDriverVersion();

    if (device.GetDriverID() == VK_DRIVER_ID_NVIDIA_PROPRIETARY_KHR) {
        const u32 major = (version >> 22) & 0x3ff;
        const u32 minor = (version >> 14) & 0x0ff;
        const u32 secondary = (version >> 6) & 0x0ff;
        const u32 tertiary = version & 0x003f;
        return fmt::format("{}.{}.{}.{}", major, minor, secondary, tertiary);
    }
    if (device.GetDriverID() == VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS_KHR) {
        const u32 major = version >> 14;
        const u32 minor = version & 0x3fff;
        return fmt::format("{}.{}", major, minor);
    }

    return GetReadableVersion(version);
}

std::string BuildCommaSeparatedExtensions(std::vector<std::string> available_extensions) {
    std::sort(std::begin(available_extensions), std::end(available_extensions));

    static constexpr std::size_t AverageExtensionSize = 64;
    std::string separated_extensions;
    separated_extensions.reserve(available_extensions.size() * AverageExtensionSize);

    const auto end = std::end(available_extensions);
    for (auto extension = std::begin(available_extensions); extension != end; ++extension) {
        if (const bool is_last = extension + 1 == end; is_last) {
            separated_extensions += *extension;
        } else {
            separated_extensions += fmt::format("{},", *extension);
        }
    }
    return separated_extensions;
}

} // Anonymous namespace

RendererVulkan::RendererVulkan(Core::Frontend::EmuWindow& window, Core::System& system)
    : RendererBase(window), system{system} {}

RendererVulkan::~RendererVulkan() {
    ShutDown();
}

void RendererVulkan::SwapBuffers(const Tegra::FramebufferConfig* framebuffer) {
    render_window.PollEvents();

    if (!framebuffer) {
        return;
    }

    const auto& layout = render_window.GetFramebufferLayout();
    if (layout.width > 0 && layout.height > 0 && render_window.IsShown()) {
        const VAddr framebuffer_addr = framebuffer->address + framebuffer->offset;
        const bool use_accelerated =
            rasterizer->AccelerateDisplay(*framebuffer, framebuffer_addr, framebuffer->stride);
        const bool is_srgb = use_accelerated && screen_info.is_srgb;
        if (swapchain->HasFramebufferChanged(layout) || swapchain->GetSrgbState() != is_srgb) {
            swapchain->Create(layout.width, layout.height, is_srgb);
            blit_screen->Recreate();
        }

        scheduler->WaitWorker();

        swapchain->AcquireNextImage();
        const auto [fence, render_semaphore] = blit_screen->Draw(*framebuffer, use_accelerated);

        scheduler->Flush(false, render_semaphore);

        if (swapchain->Present(render_semaphore, fence)) {
            blit_screen->Recreate();
        }

        rasterizer->TickFrame();
    }

    render_window.PollEvents();
}

bool RendererVulkan::TryPresent(int /*timeout_ms*/) {
    // TODO (bunnei): ImplementMe
    return true;
}

bool RendererVulkan::Init() {
    library = OpenVulkanLibrary();
    instance = CreateInstance(library, dld, render_window.GetWindowInfo().type,
                              Settings::values.renderer_debug);
    if (!instance || !CreateDebugCallback() || !CreateSurface() || !PickDevices()) {
        return false;
    }

    Report();

    memory_manager = std::make_unique<VKMemoryManager>(*device);

    resource_manager = std::make_unique<VKResourceManager>(*device);

    const auto& framebuffer = render_window.GetFramebufferLayout();
    swapchain = std::make_unique<VKSwapchain>(*surface, *device);
    swapchain->Create(framebuffer.width, framebuffer.height, false);

    state_tracker = std::make_unique<StateTracker>(system);

    scheduler = std::make_unique<VKScheduler>(*device, *resource_manager, *state_tracker);

    rasterizer = std::make_unique<RasterizerVulkan>(system, render_window, screen_info, *device,
                                                    *resource_manager, *memory_manager,
                                                    *state_tracker, *scheduler);

    blit_screen = std::make_unique<VKBlitScreen>(system, render_window, *rasterizer, *device,
                                                 *resource_manager, *memory_manager, *swapchain,
                                                 *scheduler, screen_info);

    return true;
}

void RendererVulkan::ShutDown() {
    if (!device) {
        return;
    }
    if (const auto& dev = device->GetLogical()) {
        dev.WaitIdle();
    }

    rasterizer.reset();
    blit_screen.reset();
    scheduler.reset();
    swapchain.reset();
    memory_manager.reset();
    resource_manager.reset();
    device.reset();
}

bool RendererVulkan::CreateDebugCallback() {
    if (!Settings::values.renderer_debug) {
        return true;
    }
    debug_callback = instance.TryCreateDebugCallback(DebugCallback);
    if (!debug_callback) {
        LOG_ERROR(Render_Vulkan, "Failed to create debug callback");
        return false;
    }
    return true;
}

bool RendererVulkan::CreateSurface() {
    [[maybe_unused]] const auto& window_info = render_window.GetWindowInfo();
    VkSurfaceKHR unsafe_surface = nullptr;

#ifdef _WIN32
    if (window_info.type == Core::Frontend::WindowSystemType::Windows) {
        const HWND hWnd = static_cast<HWND>(window_info.render_surface);
        const VkWin32SurfaceCreateInfoKHR win32_ci{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
                                                   nullptr, 0, nullptr, hWnd};
        const auto vkCreateWin32SurfaceKHR = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(
            dld.vkGetInstanceProcAddr(*instance, "vkCreateWin32SurfaceKHR"));
        if (!vkCreateWin32SurfaceKHR ||
            vkCreateWin32SurfaceKHR(*instance, &win32_ci, nullptr, &unsafe_surface) != VK_SUCCESS) {
            LOG_ERROR(Render_Vulkan, "Failed to initialize Win32 surface");
            return false;
        }
    }
#endif
#if !defined(_WIN32) && !defined(__APPLE__)
    if (window_info.type == Core::Frontend::WindowSystemType::X11) {
        const VkXlibSurfaceCreateInfoKHR xlib_ci{
            VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR, nullptr, 0,
            static_cast<Display*>(window_info.display_connection),
            reinterpret_cast<Window>(window_info.render_surface)};
        const auto vkCreateXlibSurfaceKHR = reinterpret_cast<PFN_vkCreateXlibSurfaceKHR>(
            dld.vkGetInstanceProcAddr(*instance, "vkCreateXlibSurfaceKHR"));
        if (!vkCreateXlibSurfaceKHR ||
            vkCreateXlibSurfaceKHR(*instance, &xlib_ci, nullptr, &unsafe_surface) != VK_SUCCESS) {
            LOG_ERROR(Render_Vulkan, "Failed to initialize Xlib surface");
            return false;
        }
    }
    if (window_info.type == Core::Frontend::WindowSystemType::Wayland) {
        const VkWaylandSurfaceCreateInfoKHR wayland_ci{
            VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR, nullptr, 0,
            static_cast<wl_display*>(window_info.display_connection),
            static_cast<wl_surface*>(window_info.render_surface)};
        const auto vkCreateWaylandSurfaceKHR = reinterpret_cast<PFN_vkCreateWaylandSurfaceKHR>(
            dld.vkGetInstanceProcAddr(*instance, "vkCreateWaylandSurfaceKHR"));
        if (!vkCreateWaylandSurfaceKHR ||
            vkCreateWaylandSurfaceKHR(*instance, &wayland_ci, nullptr, &unsafe_surface) !=
                VK_SUCCESS) {
            LOG_ERROR(Render_Vulkan, "Failed to initialize Wayland surface");
            return false;
        }
    }
#endif
    if (!unsafe_surface) {
        LOG_ERROR(Render_Vulkan, "Presentation not supported on this platform");
        return false;
    }

    surface = vk::SurfaceKHR(unsafe_surface, *instance, dld);
    return true;
}

bool RendererVulkan::PickDevices() {
    const auto devices = instance.EnumeratePhysicalDevices();
    if (!devices) {
        LOG_ERROR(Render_Vulkan, "Failed to enumerate physical devices");
        return false;
    }

    const s32 device_index = Settings::values.vulkan_device;
    if (device_index < 0 || device_index >= static_cast<s32>(devices->size())) {
        LOG_ERROR(Render_Vulkan, "Invalid device index {}!", device_index);
        return false;
    }
    const vk::PhysicalDevice physical_device((*devices)[static_cast<std::size_t>(device_index)],
                                             dld);
    if (!VKDevice::IsSuitable(physical_device, *surface)) {
        return false;
    }

    device = std::make_unique<VKDevice>(*instance, physical_device, *surface, dld);
    return device->Create();
}

void RendererVulkan::Report() const {
    const std::string vendor_name{device->GetVendorName()};
    const std::string model_name{device->GetModelName()};
    const std::string driver_version = GetDriverVersion(*device);
    const std::string driver_name = fmt::format("{} {}", vendor_name, driver_version);

    const std::string api_version = GetReadableVersion(device->GetApiVersion());

    const std::string extensions = BuildCommaSeparatedExtensions(device->GetAvailableExtensions());

    LOG_INFO(Render_Vulkan, "Driver: {}", driver_name);
    LOG_INFO(Render_Vulkan, "Device: {}", model_name);
    LOG_INFO(Render_Vulkan, "Vulkan: {}", api_version);

    auto& telemetry_session = system.TelemetrySession();
    constexpr auto field = Telemetry::FieldType::UserSystem;
    telemetry_session.AddField(field, "GPU_Vendor", vendor_name);
    telemetry_session.AddField(field, "GPU_Model", model_name);
    telemetry_session.AddField(field, "GPU_Vulkan_Driver", driver_name);
    telemetry_session.AddField(field, "GPU_Vulkan_Version", api_version);
    telemetry_session.AddField(field, "GPU_Vulkan_Extensions", extensions);
}

std::vector<std::string> RendererVulkan::EnumerateDevices() {
    vk::InstanceDispatch dld;
    Common::DynamicLibrary library = OpenVulkanLibrary();
    vk::Instance instance = CreateInstance(library, dld);
    if (!instance) {
        return {};
    }

    const std::optional physical_devices = instance.EnumeratePhysicalDevices();
    if (!physical_devices) {
        return {};
    }

    std::vector<std::string> names;
    names.reserve(physical_devices->size());
    for (const auto& device : *physical_devices) {
        names.push_back(vk::PhysicalDevice(device, dld).GetProperties().deviceName);
    }
    return names;
}

} // namespace Vulkan
