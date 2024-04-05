// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/frontend/emu_window.h"
#include "core/frontend/graphics_context.h"
#include "video_core/capture.h"
#include "video_core/renderer_metal/renderer_metal.h"
#include "video_core/renderer_metal/mtl_device.h"

namespace Metal {

RendererMetal::RendererMetal(Core::Frontend::EmuWindow& emu_window,
                             Tegra::MaxwellDeviceMemoryManager& device_memory_, Tegra::GPU& gpu_,
                             std::unique_ptr<Core::Frontend::GraphicsContext> context_)
    : RendererBase(emu_window, std::move(context_)), device_memory{device_memory_},
      gpu{gpu_}, device{},
      command_recorder(device),
      swap_chain(device, command_recorder,
                 static_cast<const CAMetalLayer*>(render_window.GetWindowInfo().render_surface)),
      rasterizer(gpu_, device, swap_chain) {}

RendererMetal::~RendererMetal() = default;

void RendererMetal::Composite(std::span<const Tegra::FramebufferConfig> framebuffers) {
    if (framebuffers.empty()) {
        return;
    }

    // HACK
    swap_chain.AcquireNextDrawable();

    MTLRenderPassDescriptor* render_pass_descriptor = [MTLRenderPassDescriptor renderPassDescriptor];
    render_pass_descriptor.colorAttachments[0].clearColor = MTLClearColorMake(1.0, 0.5, 0.0, 1.0);
    render_pass_descriptor.colorAttachments[0].loadAction  = MTLLoadActionClear;
    render_pass_descriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    render_pass_descriptor.colorAttachments[0].texture = swap_chain.GetDrawableTexture();

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
