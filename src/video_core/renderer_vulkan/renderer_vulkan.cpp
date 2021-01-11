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
#include "video_core/renderer_vulkan/vk_master_semaphore.h"
#include "video_core/renderer_vulkan/vk_memory_manager.h"
#include "video_core/renderer_vulkan/vk_rasterizer.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_state_tracker.h"
#include "video_core/renderer_vulkan/vk_swapchain.h"
#include "video_core/vulkan_common/vulkan_debug_callback.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_instance.h"
#include "video_core/vulkan_common/vulkan_library.h"
#include "video_core/vulkan_common/vulkan_surface.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {
namespace {
std::string GetReadableVersion(u32 version) {
    return fmt::format("{}.{}.{}", VK_VERSION_MAJOR(version), VK_VERSION_MINOR(version),
                       VK_VERSION_PATCH(version));
}

std::string GetDriverVersion(const Device& device) {
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

RendererVulkan::RendererVulkan(Core::TelemetrySession& telemetry_session_,
                               Core::Frontend::EmuWindow& emu_window,
                               Core::Memory::Memory& cpu_memory_, Tegra::GPU& gpu_,
                               std::unique_ptr<Core::Frontend::GraphicsContext> context_)
    : RendererBase{emu_window, std::move(context_)}, telemetry_session{telemetry_session_},
      cpu_memory{cpu_memory_}, gpu{gpu_} {}

RendererVulkan::~RendererVulkan() {
    ShutDown();
}

void RendererVulkan::SwapBuffers(const Tegra::FramebufferConfig* framebuffer) {
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
        const VkSemaphore render_semaphore = blit_screen->Draw(*framebuffer, use_accelerated);

        scheduler->Flush(render_semaphore);

        if (swapchain->Present(render_semaphore)) {
            blit_screen->Recreate();
        }

        rasterizer->TickFrame();
    }

    render_window.OnFrameDisplayed();
}

bool RendererVulkan::Init() try {
    library = OpenLibrary();
    instance = CreateInstance(library, dld, VK_API_VERSION_1_1, render_window.GetWindowInfo().type,
                              true, Settings::values.renderer_debug);
    if (Settings::values.renderer_debug) {
        debug_callback = CreateDebugCallback(instance);
    }
    surface = CreateSurface(instance, render_window);

    InitializeDevice();
    Report();

    memory_manager = std::make_unique<VKMemoryManager>(*device);

    state_tracker = std::make_unique<StateTracker>(gpu);

    scheduler = std::make_unique<VKScheduler>(*device, *state_tracker);

    const auto& framebuffer = render_window.GetFramebufferLayout();
    swapchain = std::make_unique<VKSwapchain>(*surface, *device, *scheduler);
    swapchain->Create(framebuffer.width, framebuffer.height, false);

    rasterizer = std::make_unique<RasterizerVulkan>(render_window, gpu, gpu.MemoryManager(),
                                                    cpu_memory, screen_info, *device,
                                                    *memory_manager, *state_tracker, *scheduler);

    blit_screen =
        std::make_unique<VKBlitScreen>(cpu_memory, render_window, *rasterizer, *device,
                                       *memory_manager, *swapchain, *scheduler, screen_info);
    return true;

} catch (const vk::Exception& exception) {
    LOG_ERROR(Render_Vulkan, "Vulkan initialization failed with error: {}", exception.what());
    return false;
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
    device.reset();
}

void RendererVulkan::InitializeDevice() {
    const std::vector<VkPhysicalDevice> devices = instance.EnumeratePhysicalDevices();
    const s32 device_index = Settings::values.vulkan_device.GetValue();
    if (device_index < 0 || device_index >= static_cast<s32>(devices.size())) {
        LOG_ERROR(Render_Vulkan, "Invalid device index {}!", device_index);
        throw vk::Exception(VK_ERROR_INITIALIZATION_FAILED);
    }
    const vk::PhysicalDevice physical_device(devices[static_cast<size_t>(device_index)], dld);
    device = std::make_unique<Device>(*instance, physical_device, *surface, dld);
}

void RendererVulkan::Report() const {
    const std::string vendor_name{device->GetVendorName()};
    const std::string model_name{device->GetModelName()};
    const std::string driver_version = GetDriverVersion(*device);
    const std::string driver_name = fmt::format("{} {}", vendor_name, driver_version);

    const std::string api_version = GetReadableVersion(device->ApiVersion());

    const std::string extensions = BuildCommaSeparatedExtensions(device->GetAvailableExtensions());

    LOG_INFO(Render_Vulkan, "Driver: {}", driver_name);
    LOG_INFO(Render_Vulkan, "Device: {}", model_name);
    LOG_INFO(Render_Vulkan, "Vulkan: {}", api_version);

    static constexpr auto field = Common::Telemetry::FieldType::UserSystem;
    telemetry_session.AddField(field, "GPU_Vendor", vendor_name);
    telemetry_session.AddField(field, "GPU_Model", model_name);
    telemetry_session.AddField(field, "GPU_Vulkan_Driver", driver_name);
    telemetry_session.AddField(field, "GPU_Vulkan_Version", api_version);
    telemetry_session.AddField(field, "GPU_Vulkan_Extensions", extensions);
}

std::vector<std::string> RendererVulkan::EnumerateDevices() try {
    vk::InstanceDispatch dld;
    const Common::DynamicLibrary library = OpenLibrary();
    const vk::Instance instance = CreateInstance(library, dld, VK_API_VERSION_1_0);
    const std::vector<VkPhysicalDevice> physical_devices = instance.EnumeratePhysicalDevices();
    std::vector<std::string> names;
    names.reserve(physical_devices.size());
    for (const VkPhysicalDevice device : physical_devices) {
        names.push_back(vk::PhysicalDevice(device, dld).GetProperties().deviceName);
    }
    return names;

} catch (const vk::Exception& exception) {
    LOG_ERROR(Render_Vulkan, "Failed to enumerate devices with error: {}", exception.what());
    return {};
}

} // namespace Vulkan
