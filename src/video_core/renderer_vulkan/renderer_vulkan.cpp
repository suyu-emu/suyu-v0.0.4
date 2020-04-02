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

#include "common/assert.h"
#include "common/dynamic_library.h"
#include "common/logging/log.h"
#include "common/telemetry.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/frontend/emu_window.h"
#include "core/memory.h"
#include "core/perf_stats.h"
#include "core/settings.h"
#include "core/telemetry_session.h"
#include "video_core/gpu.h"
#include "video_core/renderer_vulkan/declarations.h"
#include "video_core/renderer_vulkan/renderer_vulkan.h"
#include "video_core/renderer_vulkan/vk_blit_screen.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_memory_manager.h"
#include "video_core/renderer_vulkan/vk_rasterizer.h"
#include "video_core/renderer_vulkan/vk_resource_manager.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_state_tracker.h"
#include "video_core/renderer_vulkan/vk_swapchain.h"

// Include these late to avoid changing Vulkan-Hpp's dynamic dispatcher size
#ifdef _WIN32
#include <windows.h>
// ensure include order
#include <vulkan/vulkan_win32.h>
#endif

#ifdef __linux__
#include <X11/Xlib.h>
#include <vulkan/vulkan_wayland.h>
#include <vulkan/vulkan_xlib.h>
#endif

namespace Vulkan {

namespace {

using Core::Frontend::WindowSystemType;

VkBool32 DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity_,
                       VkDebugUtilsMessageTypeFlagsEXT type,
                       const VkDebugUtilsMessengerCallbackDataEXT* data,
                       [[maybe_unused]] void* user_data) {
    const auto severity{static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(severity_)};
    const char* message{data->pMessage};

    if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) {
        LOG_CRITICAL(Render_Vulkan, "{}", message);
    } else if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
        LOG_WARNING(Render_Vulkan, "{}", message);
    } else if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo) {
        LOG_INFO(Render_Vulkan, "{}", message);
    } else if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose) {
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
        std::string filename = File::GetBundleDirectory() + "/Contents/Frameworks/libvulkan.dylib";
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

UniqueInstance CreateInstance(Common::DynamicLibrary& library, vk::DispatchLoaderDynamic& dld,
                              WindowSystemType window_type = WindowSystemType::Headless,
                              bool enable_layers = false) {
    if (!library.IsOpen()) {
        LOG_ERROR(Render_Vulkan, "Vulkan library not available");
        return UniqueInstance{};
    }
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
    if (!library.GetSymbol("vkGetInstanceProcAddr", &vkGetInstanceProcAddr)) {
        LOG_ERROR(Render_Vulkan, "vkGetInstanceProcAddr not present in Vulkan");
        return UniqueInstance{};
    }
    dld.init(vkGetInstanceProcAddr);

    std::vector<const char*> extensions;
    extensions.reserve(4);
    switch (window_type) {
    case Core::Frontend::WindowSystemType::Headless:
        break;
#ifdef _WIN32
    case Core::Frontend::WindowSystemType::Windows:
        extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
        break;
#endif
#ifdef __linux__
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

    u32 num_properties;
    if (vk::enumerateInstanceExtensionProperties(nullptr, &num_properties, nullptr, dld) !=
        vk::Result::eSuccess) {
        LOG_ERROR(Render_Vulkan, "Failed to query number of extension properties");
        return UniqueInstance{};
    }
    std::vector<vk::ExtensionProperties> properties(num_properties);
    if (vk::enumerateInstanceExtensionProperties(nullptr, &num_properties, properties.data(),
                                                 dld) != vk::Result::eSuccess) {
        LOG_ERROR(Render_Vulkan, "Failed to query extension properties");
        return UniqueInstance{};
    }

    for (const char* extension : extensions) {
        const auto it =
            std::find_if(properties.begin(), properties.end(), [extension](const auto& prop) {
                return !std::strcmp(extension, prop.extensionName);
            });
        if (it == properties.end()) {
            LOG_ERROR(Render_Vulkan, "Required instance extension {} is not available", extension);
            return UniqueInstance{};
        }
    }

    const vk::ApplicationInfo application_info("yuzu Emulator", VK_MAKE_VERSION(0, 1, 0),
                                               "yuzu Emulator", VK_MAKE_VERSION(0, 1, 0),
                                               VK_API_VERSION_1_1);
    const std::array layers = {"VK_LAYER_LUNARG_standard_validation"};
    const vk::InstanceCreateInfo instance_ci(
        {}, &application_info, enable_layers ? static_cast<u32>(layers.size()) : 0, layers.data(),
        static_cast<u32>(extensions.size()), extensions.data());
    vk::Instance unsafe_instance;
    if (vk::createInstance(&instance_ci, nullptr, &unsafe_instance, dld) != vk::Result::eSuccess) {
        LOG_ERROR(Render_Vulkan, "Failed to create Vulkan instance");
        return UniqueInstance{};
    }
    dld.init(unsafe_instance);
    return UniqueInstance(unsafe_instance, {nullptr, dld});
}

std::string GetReadableVersion(u32 version) {
    return fmt::format("{}.{}.{}", VK_VERSION_MAJOR(version), VK_VERSION_MINOR(version),
                       VK_VERSION_PATCH(version));
}

std::string GetDriverVersion(const VKDevice& device) {
    // Extracted from
    // https://github.com/SaschaWillems/vulkan.gpuinfo.org/blob/5dddea46ea1120b0df14eef8f15ff8e318e35462/functions.php#L308-L314
    const u32 version = device.GetDriverVersion();

    if (device.GetDriverID() == vk::DriverIdKHR::eNvidiaProprietary) {
        const u32 major = (version >> 22) & 0x3ff;
        const u32 minor = (version >> 14) & 0x0ff;
        const u32 secondary = (version >> 6) & 0x0ff;
        const u32 tertiary = version & 0x003f;
        return fmt::format("{}.{}.{}.{}", major, minor, secondary, tertiary);
    }
    if (device.GetDriverID() == vk::DriverIdKHR::eIntelProprietaryWindows) {
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
    const auto dev = device->GetLogical();
    const auto& dld = device->GetDispatchLoader();
    if (dev && dld.vkDeviceWaitIdle) {
        dev.waitIdle(dld);
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
    const vk::DebugUtilsMessengerCreateInfoEXT callback_ci(
        {},
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose,
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
        &DebugCallback, nullptr);
    vk::DebugUtilsMessengerEXT unsafe_callback;
    if (instance->createDebugUtilsMessengerEXT(&callback_ci, nullptr, &unsafe_callback, dld) !=
        vk::Result::eSuccess) {
        LOG_ERROR(Render_Vulkan, "Failed to create debug callback");
        return false;
    }
    debug_callback = UniqueDebugUtilsMessengerEXT(unsafe_callback, {*instance, nullptr, dld});
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
        if (!vkCreateWin32SurfaceKHR || vkCreateWin32SurfaceKHR(instance.get(), &win32_ci, nullptr,
                                                                &unsafe_surface) != VK_SUCCESS) {
            LOG_ERROR(Render_Vulkan, "Failed to initialize Win32 surface");
            return false;
        }
    }
#endif
#ifdef __linux__
    if (window_info.type == Core::Frontend::WindowSystemType::X11) {
        const VkXlibSurfaceCreateInfoKHR xlib_ci{
            VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR, nullptr, 0,
            static_cast<Display*>(window_info.display_connection),
            reinterpret_cast<Window>(window_info.render_surface)};
        const auto vkCreateXlibSurfaceKHR = reinterpret_cast<PFN_vkCreateXlibSurfaceKHR>(
            dld.vkGetInstanceProcAddr(*instance, "vkCreateXlibSurfaceKHR"));
        if (!vkCreateXlibSurfaceKHR || vkCreateXlibSurfaceKHR(instance.get(), &xlib_ci, nullptr,
                                                              &unsafe_surface) != VK_SUCCESS) {
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
            vkCreateWaylandSurfaceKHR(instance.get(), &wayland_ci, nullptr, &unsafe_surface) !=
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

    surface = UniqueSurfaceKHR(unsafe_surface, {*instance, nullptr, dld});
    return true;
}

bool RendererVulkan::PickDevices() {
    const auto devices = instance->enumeratePhysicalDevices(dld);

    const s32 device_index = Settings::values.vulkan_device;
    if (device_index < 0 || device_index >= static_cast<s32>(devices.size())) {
        LOG_ERROR(Render_Vulkan, "Invalid device index {}!", device_index);
        return false;
    }
    const vk::PhysicalDevice physical_device = devices[static_cast<std::size_t>(device_index)];

    if (!VKDevice::IsSuitable(physical_device, *surface, dld)) {
        return false;
    }

    device = std::make_unique<VKDevice>(dld, physical_device, *surface);
    return device->Create(*instance);
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
    // Avoid putting DispatchLoaderDynamic, it's too large
    auto dld_memory = std::make_unique<vk::DispatchLoaderDynamic>();
    auto& dld = *dld_memory;

    Common::DynamicLibrary library = OpenVulkanLibrary();
    UniqueInstance instance = CreateInstance(library, dld);
    if (!instance) {
        return {};
    }

    u32 num_devices;
    if (instance->enumeratePhysicalDevices(&num_devices, nullptr, dld) != vk::Result::eSuccess) {
        return {};
    }
    std::vector<vk::PhysicalDevice> devices(num_devices);
    if (instance->enumeratePhysicalDevices(&num_devices, devices.data(), dld) !=
        vk::Result::eSuccess) {
        return {};
    }

    std::vector<std::string> names;
    names.reserve(num_devices);
    for (auto& device : devices) {
        names.push_back(device.getProperties(dld).deviceName);
    }
    return names;
}

} // namespace Vulkan
