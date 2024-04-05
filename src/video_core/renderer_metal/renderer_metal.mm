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
                   layer(static_cast<const CAMetalLayer*>(render_window.GetWindowInfo().render_surface)),
                   rasterizer(gpu_, device, layer) {
    // HACK
    MTLTextureDescriptor* textureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm_sRGB
                                                                                                 width:1280
                                                                                                height:720
                                                                                             mipmapped:NO];
    renderTexture = [device.GetDevice() newTextureWithDescriptor:textureDescriptor];
}

RendererMetal::~RendererMetal() = default;

void RendererMetal::Composite(std::span<const Tegra::FramebufferConfig> framebuffers) {
    if (framebuffers.empty()) {
        return;
    }

    // HACK
    @autoreleasepool {
        //id<CAMetalDrawable> drawable = [layer nextDrawable];

        MTLRenderPassDescriptor* renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
        renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(1.0, 0.5, 0.0, 1.0);
        renderPassDescriptor.colorAttachments[0].loadAction  = MTLLoadActionClear;
        renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
        renderPassDescriptor.colorAttachments[0].texture = renderTexture;//drawable.texture;

        id<MTLCommandBuffer> commandBuffer = [device.GetCommandQueue() commandBuffer];
        id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        [renderEncoder endEncoding];
        //[commandBuffer presentDrawable:drawable];
        [commandBuffer commit];
    }

    gpu.RendererFrameEndNotify();
    rasterizer.TickFrame();

    render_window.OnFrameDisplayed();
}

std::vector<u8> RendererMetal::GetAppletCaptureBuffer() {
    return std::vector<u8>(VideoCore::Capture::TiledSize);
}

} // namespace Metal
