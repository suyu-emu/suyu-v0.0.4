// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/frontend/emu_window.h"
#include "core/frontend/graphics_context.h"
#include "video_core/capture.h"
#include "video_core/renderer_metal/mtl_device.h"
#include "video_core/renderer_metal/renderer_metal.h"

// TODO: Make it the way that we can make a swiftUI for the macOS app instead of qt

namespace Metal {

RendererMetal::RendererMetal(Core::Frontend::EmuWindow& emu_window,
                             Tegra::MaxwellDeviceMemoryManager& device_memory_, Tegra::GPU& gpu_,
                             std::unique_ptr<Core::Frontend::GraphicsContext> context_)
    : RendererBase(emu_window, std::move(context_)),
      device_memory{device_memory_}, gpu{gpu_}, device{}, command_recorder(device),
      swap_chain(device, command_recorder,
                 static_cast<CA::MetalLayer*>(render_window.GetWindowInfo().render_surface)),
      rasterizer(gpu_, device_memory, device, command_recorder, swap_chain) {
    CreateBlitPipelineState();
}

RendererMetal::~RendererMetal() {
    blit_pipeline_state->release();
    blit_sampler_state->release();
}

void RendererMetal::Composite(std::span<const Tegra::FramebufferConfig> framebuffers) {
    if (framebuffers.empty()) {
        return;
    }

    // Ask the swap chain to get next drawable
    swap_chain.AcquireNextDrawable();

    // TODO: copy the framebuffer to the drawable texture instead of this dummy render pass
    MTL::RenderPassDescriptor* render_pass_descriptor = MTL::RenderPassDescriptor::alloc()->init();
    render_pass_descriptor->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionDontCare);
    render_pass_descriptor->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
    render_pass_descriptor->colorAttachments()->object(0)->setTexture(
        swap_chain.GetDrawableTexture());

    command_recorder.BeginRenderPass(render_pass_descriptor);

    // Blit the framebuffer to the drawable texture
    // TODO: acquire the texture from @ref framebuffers
    const Framebuffer* const framebuffer = rasterizer.texture_cache.GetFramebuffer();
    if (!framebuffer) {
        return;
    }
    MTL::Texture* src_texture = framebuffer->GetHandle()->colorAttachments()->object(0)->texture();
    command_recorder.GetRenderCommandEncoder()->setRenderPipelineState(blit_pipeline_state);
    command_recorder.GetRenderCommandEncoder()->setFragmentTexture(src_texture, 0);
    command_recorder.GetRenderCommandEncoder()->setFragmentSamplerState(blit_sampler_state, 0);

    // Draw a full screen triangle which will get clipped to a rectangle
    command_recorder.GetRenderCommandEncoder()->drawPrimitives(MTL::PrimitiveTypeTriangle,
                                                               NS::UInteger(0), NS::UInteger(3));

    swap_chain.Present();
    command_recorder.Submit();

    gpu.RendererFrameEndNotify();
    rasterizer.TickFrame();

    render_window.OnFrameDisplayed();
}

std::vector<u8> RendererMetal::GetAppletCaptureBuffer() {
    return std::vector<u8>(VideoCore::Capture::TiledSize);
}

void RendererMetal::CreateBlitPipelineState() {
    MTL::CompileOptions* compile_options = MTL::CompileOptions::alloc()->init();
    NS::Error* error = nullptr;
    MTL::Library* library = device.GetDevice()->newLibrary(NS::String::string(
                                                               R"(
        #include <metal_stdlib>
        using namespace metal;

        constant float2 texCoords[] = {
            float2(0.0, -1.0),
            float2(0.0,  1.0),
            float2(2.0,  1.0),
        };

        struct VertexOut {
            float4 position [[position]];
            float2 texCoord;
        };

        vertex VertexOut vertexMain(uint vid [[vertex_id]]) {
            VertexOut out;
            out.position = float4(texCoords[vid] * 2.0 - 1.0, 0.0, 1.0);
            out.texCoord = texCoords[vid];

            return out;
        }

        fragment float4 fragmentMain(VertexOut in [[stage_in]],
                                      texture2d<float> texture [[texture(0)]],
                                      sampler sampler [[sampler(0)]]) {
            return texture.sample(sampler, in.texCoord);
        }
    )",
                                                               NS::ASCIIStringEncoding),
                                                           compile_options, &error);
    if (error) {
        LOG_ERROR(Render_Metal, "failed to create blit library: {}",
                  error->description()->cString(NS::ASCIIStringEncoding));
    }

    MTL::Function* vertex_function =
        library->newFunction(NS::String::string("vertexMain", NS::ASCIIStringEncoding));
    MTL::Function* fragment_function =
        library->newFunction(NS::String::string("fragmentMain", NS::ASCIIStringEncoding));

    MTL::RenderPipelineDescriptor* pipeline_descriptor =
        MTL::RenderPipelineDescriptor::alloc()->init();
    pipeline_descriptor->setVertexFunction(vertex_function);
    pipeline_descriptor->setFragmentFunction(fragment_function);
    // TODO: get the pixel format from metal layer
    pipeline_descriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatRGBA8Unorm);

    error = nullptr;
    blit_pipeline_state = device.GetDevice()->newRenderPipelineState(pipeline_descriptor, &error);
    if (error) {
        LOG_ERROR(Render_Metal, "failed to create blit pipeline state: {}",
                  error->description()->cString(NS::ASCIIStringEncoding));
    }

    // Create sampler state
    MTL::SamplerDescriptor* sampler_descriptor = MTL::SamplerDescriptor::alloc()->init();
    sampler_descriptor->setMinFilter(MTL::SamplerMinMagFilterLinear);
    sampler_descriptor->setMagFilter(MTL::SamplerMinMagFilterLinear);

    blit_sampler_state = device.GetDevice()->newSamplerState(sampler_descriptor);

    // Deallocate unnecessary objects
    fragment_function->release();
    vertex_function->release();
    library->release();
}

} // namespace Metal
