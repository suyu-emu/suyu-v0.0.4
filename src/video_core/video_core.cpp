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
std::unique_ptr<VideoCore::RendererBase> CreateRenderer(Core::Frontend::EmuWindow& emu_window,
                                                        Core::System& system,
                                                        Core::Frontend::GraphicsContext& context) {
    switch (Settings::values.renderer_backend.GetValue()) {
    case Settings::RendererBackend::OpenGL:
        return std::make_unique<OpenGL::RendererOpenGL>(emu_window, system, context);
#ifdef HAS_VULKAN
    case Settings::RendererBackend::Vulkan:
        return std::make_unique<Vulkan::RendererVulkan>(emu_window, system);
#endif
    default:
        return nullptr;
    }
}
} // Anonymous namespace

namespace VideoCore {

std::unique_ptr<Tegra::GPU> CreateGPU(Core::Frontend::EmuWindow& emu_window, Core::System& system) {
    auto context = emu_window.CreateSharedContext();
    const auto scope = context->Acquire();
    auto renderer = CreateRenderer(emu_window, system, *context);
    if (!renderer->Init()) {
        return nullptr;
    }

    if (Settings::values.use_asynchronous_gpu_emulation.GetValue()) {
        return std::make_unique<VideoCommon::GPUAsynch>(system, std::move(renderer),
                                                        std::move(context));
    }
    return std::make_unique<VideoCommon::GPUSynch>(system, std::move(renderer), std::move(context));
}

u16 GetResolutionScaleFactor(const RendererBase& renderer) {
    return static_cast<u16>(
        Settings::values.resolution_factor.GetValue() != 0
            ? Settings::values.resolution_factor.GetValue()
            : renderer.GetRenderWindow().GetFramebufferLayout().GetScalingRatio());
}

} // namespace VideoCore
