// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "common/logging/log.h"
#include "core/core.h"
#include "core/settings.h"
#include "video_core/gpu_asynch.h"
#include "video_core/gpu_synch.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_opengl/renderer_opengl.h"
#ifdef HAS_VULKAN
#include "video_core/renderer_vulkan/renderer_vulkan.h"
#endif
#include "video_core/video_core.h"

namespace {

std::unique_ptr<VideoCore::RendererBase> CreateRenderer(
    Core::System& system, Core::Frontend::EmuWindow& emu_window, Tegra::GPU& gpu,
    std::unique_ptr<Core::Frontend::GraphicsContext> context) {
    switch (Settings::values.renderer_backend.GetValue()) {
    case Settings::RendererBackend::OpenGL:
        return std::make_unique<OpenGL::RendererOpenGL>(system, emu_window, gpu,
                                                        std::move(context));
#ifdef HAS_VULKAN
    case Settings::RendererBackend::Vulkan:
        return std::make_unique<Vulkan::RendererVulkan>(system, emu_window, gpu,
                                                        std::move(context));
#endif
    default:
        return nullptr;
    }
}

} // Anonymous namespace

namespace VideoCore {

std::unique_ptr<Tegra::GPU> CreateGPU(Core::Frontend::EmuWindow& emu_window, Core::System& system) {
    std::unique_ptr<Tegra::GPU> gpu;
    if (Settings::values.use_asynchronous_gpu_emulation.GetValue()) {
        gpu = std::make_unique<VideoCommon::GPUAsynch>(system);
    } else {
        gpu = std::make_unique<VideoCommon::GPUSynch>(system);
    }

    auto context = emu_window.CreateSharedContext();
    const auto scope = context->Acquire();

    auto renderer = CreateRenderer(system, emu_window, *gpu, std::move(context));
    if (!renderer->Init()) {
        return nullptr;
    }

    gpu->BindRenderer(std::move(renderer));
    return gpu;
}

u16 GetResolutionScaleFactor(const RendererBase& renderer) {
    return static_cast<u16>(
        Settings::values.resolution_factor.GetValue() != 0
            ? Settings::values.resolution_factor.GetValue()
            : renderer.GetRenderWindow().GetFramebufferLayout().GetScalingRatio());
}

} // namespace VideoCore
