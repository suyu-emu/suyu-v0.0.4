// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/frontend/emu_window.h"
#include "core/frontend/graphics_context.h"
#include "video_core/capture.h"
#include "video_core/renderer_metal/mtl_device.h"
#include "video_core/renderer_metal/renderer_metal.h"

namespace Metal {

RendererMetal::RendererMetal(Core::Frontend::EmuWindow& emu_window,
                             Tegra::MaxwellDeviceMemoryManager& device_memory_, Tegra::GPU& gpu_,
                             std::unique_ptr<Core::Frontend::GraphicsContext> context_)
    : RendererBase(emu_window, std::move(context_)), device_memory{device_memory_}, gpu{gpu_},
      device{}, command_recorder(device),
      swap_chain(device, command_recorder,
                 static_cast<CA::MetalLayer*>(render_window.GetWindowInfo().render_surface)),
      rasterizer(gpu_, device_memory, device, command_recorder, swap_chain) {}

RendererMetal::~RendererMetal() = default;

void RendererMetal::Composite(std::span<const Tegra::FramebufferConfig> framebuffers) {
    if (framebuffers.empty()) {
        return;
    }

    // Ask the swap chain to get next drawable
    swap_chain.AcquireNextDrawable();

    // TODO: copy the framebuffer to the drawable texture instead of this dummy render pass
    MTL::RenderPassDescriptor* render_pass_descriptor = MTL::RenderPassDescriptor::alloc()->init();
    render_pass_descriptor->colorAttachments()->object(0)->setClearColor(
        MTL::ClearColor::Make(1.0, 0.5, 0.0, 1.0));
    render_pass_descriptor->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
    render_pass_descriptor->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
    render_pass_descriptor->colorAttachments()->object(0)->setTexture(
        swap_chain.GetDrawableTexture());

    command_recorder.BeginRenderPass(render_pass_descriptor);

    swap_chain.Present();
    command_recorder.Submit();

    gpu.RendererFrameEndNotify();
    rasterizer.TickFrame();

    render_window.OnFrameDisplayed();
}

std::vector<u8> RendererMetal::GetAppletCaptureBuffer() {
    return std::vector<u8>(VideoCore::Capture::TiledSize);
}

} // namespace Metal
