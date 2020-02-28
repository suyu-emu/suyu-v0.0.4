// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <optional>
#include <vector>
#include "video_core/renderer_base.h"
#include "video_core/renderer_vulkan/declarations.h"

namespace Core {
class System;
}

namespace Vulkan {

class VKBlitScreen;
class VKDevice;
class VKFence;
class VKMemoryManager;
class VKResourceManager;
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
    explicit RendererVulkan(Core::Frontend::EmuWindow& window, Core::System& system);
    ~RendererVulkan() override;

    bool Init() override;
    void ShutDown() override;
    void SwapBuffers(const Tegra::FramebufferConfig* framebuffer) override;
    void TryPresent(int timeout_ms) override;

private:
    std::optional<vk::DebugUtilsMessengerEXT> CreateDebugCallback(
        const vk::DispatchLoaderDynamic& dldi);

    bool PickDevices(const vk::DispatchLoaderDynamic& dldi);

    void Report() const;

    Core::System& system;

    vk::Instance instance;
    vk::SurfaceKHR surface;

    VKScreenInfo screen_info;

    UniqueDebugUtilsMessengerEXT debug_callback;
    std::unique_ptr<VKDevice> device;
    std::unique_ptr<VKSwapchain> swapchain;
    std::unique_ptr<VKMemoryManager> memory_manager;
    std::unique_ptr<VKResourceManager> resource_manager;
    std::unique_ptr<VKScheduler> scheduler;
    std::unique_ptr<VKBlitScreen> blit_screen;
};

} // namespace Vulkan
