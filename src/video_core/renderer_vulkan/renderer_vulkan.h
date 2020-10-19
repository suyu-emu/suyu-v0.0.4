// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "common/dynamic_library.h"

#include "video_core/renderer_base.h"
#include "video_core/renderer_vulkan/wrapper.h"

namespace Core {
class TelemetrySession;
}

namespace Core::Memory {
class Memory;
}

namespace Tegra {
class GPU;
}

namespace Vulkan {

class StateTracker;
class VKBlitScreen;
class VKDevice;
class VKMemoryManager;
class VKSwapchain;
class VKScheduler;
class VKImage;

struct VKScreenInfo {
    VKImage* image{};
    u32 width{};
    u32 height{};
    bool is_srgb{};
};

class RendererVulkan final : public VideoCore::RendererBase {
public:
    explicit RendererVulkan(Core::TelemetrySession& telemtry_session,
                            Core::Frontend::EmuWindow& emu_window, Core::Memory::Memory& cpu_memory,
                            Tegra::GPU& gpu,
                            std::unique_ptr<Core::Frontend::GraphicsContext> context);
    ~RendererVulkan() override;

    bool Init() override;
    void ShutDown() override;
    void SwapBuffers(const Tegra::FramebufferConfig* framebuffer) override;

    static std::vector<std::string> EnumerateDevices();

private:
    bool CreateDebugCallback();

    bool CreateSurface();

    bool PickDevices();

    void Report() const;

    Core::TelemetrySession& telemetry_session;
    Core::Memory::Memory& cpu_memory;
    Tegra::GPU& gpu;

    Common::DynamicLibrary library;
    vk::InstanceDispatch dld;

    vk::Instance instance;
    u32 instance_version{};

    vk::SurfaceKHR surface;

    VKScreenInfo screen_info;

    vk::DebugCallback debug_callback;
    std::unique_ptr<VKDevice> device;
    std::unique_ptr<VKMemoryManager> memory_manager;
    std::unique_ptr<StateTracker> state_tracker;
    std::unique_ptr<VKScheduler> scheduler;
    std::unique_ptr<VKSwapchain> swapchain;
    std::unique_ptr<VKBlitScreen> blit_screen;
};

} // namespace Vulkan
