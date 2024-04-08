// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <span>

#include <boost/container/small_vector.hpp>
#include <boost/container/static_vector.hpp>

#include "video_core/renderer_metal/mtl_graphics_pipeline.h"

#include "common/bit_field.h"
#include "video_core/buffer_cache/buffer_cache_base.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/renderer_metal/mtl_command_recorder.h"
#include "video_core/renderer_metal/mtl_device.h"
#include "video_core/shader_notify.h"
#include "video_core/texture_cache/texture_cache_base.h"

namespace Metal {
namespace {
using Tegra::Texture::TexturePair;
using VideoCommon::NUM_RT;
using VideoCore::Surface::PixelFormat;
using VideoCore::Surface::PixelFormatFromDepthFormat;
using VideoCore::Surface::PixelFormatFromRenderTargetFormat;
using Maxwell = Tegra::Engines::Maxwell3D::Regs;
} // Anonymous namespace

GraphicsPipeline::GraphicsPipeline(const Device& device_, CommandRecorder& command_recorder_,
                                   const GraphicsPipelineCacheKey& key_, BufferCache& buffer_cache_,
                                   TextureCache& texture_cache_,
                                   VideoCore::ShaderNotify* shader_notify,
                                   std::array<MTL::Function*, NUM_STAGES> functions_,
                                   const std::array<const Shader::Info*, NUM_STAGES>& infos)
    : device{device_}, command_recorder{command_recorder_}, key{key_}, buffer_cache{buffer_cache_},
      texture_cache{texture_cache_}, functions{functions_} {
    if (shader_notify) {
        shader_notify->MarkShaderBuilding();
    }
    for (size_t stage = 0; stage < NUM_STAGES; ++stage) {
        const Shader::Info* const info{infos[stage]};
        if (!info) {
            continue;
        }
        stage_infos[stage] = *info;
    }
    Validate();
    // TODO: is the framebuffer available by this time?
    Framebuffer* framebuffer = texture_cache.GetFramebuffer();
    if (!framebuffer) {
        LOG_DEBUG(Render_Metal, "framebuffer not available");
        return;
    }
    MakePipeline(framebuffer->GetHandle());
}

void GraphicsPipeline::Configure(bool is_indexed) {
    buffer_cache.UpdateGraphicsBuffers(is_indexed);
    buffer_cache.BindHostGeometryBuffers(is_indexed);

    texture_cache.UpdateRenderTargets(true);
    const Framebuffer* const framebuffer = texture_cache.GetFramebuffer();
    if (!framebuffer) {
        return;
    }
    command_recorder.BeginOrContinueRenderPass(framebuffer->GetHandle());

    command_recorder.GetRenderCommandEncoder()->setRenderPipelineState(pipeline_state);
}

void GraphicsPipeline::MakePipeline(MTL::RenderPassDescriptor* render_pass) {
    MTL::RenderPipelineDescriptor* pipeline_descriptor =
        MTL::RenderPipelineDescriptor::alloc()->init();
    pipeline_descriptor->setVertexFunction(functions[0]);
    pipeline_descriptor->setFragmentFunction(functions[1]);
    // pipeline_descriptor->setVertexDescriptor(vertex_descriptor);
    //  TODO: get the attachment count from render pass descriptor
    for (u32 index = 0; index < NUM_RT; index++) {
        auto* render_pass_attachment = render_pass->colorAttachments()->object(index);
        // TODO: is this the correct way to check if the attachment is valid?
        if (!render_pass_attachment->texture()) {
            continue;
        }

        auto* color_attachment = pipeline_descriptor->colorAttachments()->object(index);
        color_attachment->setPixelFormat(render_pass_attachment->texture()->pixelFormat());
        // TODO: provide blend information
    }

    NS::Error* error = nullptr;
    pipeline_state = device.GetDevice()->newRenderPipelineState(pipeline_descriptor, &error);
    if (error) {
        LOG_ERROR(Render_Metal, "failed to create pipeline state: {}",
                  error->description()->cString(NS::ASCIIStringEncoding));
    }
}

void GraphicsPipeline::Validate() {
    // TODO: validate pipeline
}

} // namespace Metal
