// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vulkan/vulkan.h>
#include "core/frontend/emu_window.h"
#include "yuzu_cmd/emu_window/emu_window_sdl2.h"

class EmuWindow_SDL2_VK final : public EmuWindow_SDL2 {
public:
    explicit EmuWindow_SDL2_VK(Core::System& system, bool fullscreen);
    ~EmuWindow_SDL2_VK();

    void MakeCurrent() override;
    void DoneCurrent() override;
    void Present() override;
    void RetrieveVulkanHandlers(void* get_instance_proc_addr, void* instance,
                                void* surface) const override;

    std::unique_ptr<Core::Frontend::GraphicsContext> CreateSharedContext() const override;

private:
    bool UseStandardLayers(PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr) const;

    VkInstance vk_instance{};
    VkSurfaceKHR vk_surface{};

    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr{};
    PFN_vkDestroyInstance vkDestroyInstance{};
};
