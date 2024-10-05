// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <iostream>
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
#include "video_core/texture_cache/texture_cache.h"
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
        enabled_uniform_buffer_masks[stage] = info->constant_buffer_mask;
        std::ranges::copy(info->constant_buffer_used_sizes, uniform_buffer_sizes[stage].begin());
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
    texture_cache.SynchronizeGraphicsDescriptors();

    struct {
        std::array<VideoCommon::ImageViewInOut, 32> views;
        size_t view_index{};
    } all_views[5];
    struct {
        std::array<VideoCommon::SamplerId, 32> samplers;
        size_t sampler_index{};
    } all_samplers[5];

    // Find resources
    const auto& regs{maxwell3d->regs};
    const bool via_header_index{regs.sampler_binding == Maxwell::SamplerBinding::ViaHeaderBinding};

    const auto configure_stage{[&](u32 stage) {
        const Shader::Info& info{stage_infos[stage]};

        std::array<VideoCommon::ImageViewInOut, 32>& views{all_views[stage].views};
        std::array<VideoCommon::SamplerId, 32>& samplers{all_samplers[stage].samplers};
        size_t& view_index{all_views[stage].view_index};
        size_t& sampler_index{all_samplers[stage].sampler_index};

        texture_cache.SynchronizeGraphicsDescriptors();

        buffer_cache.SetUniformBuffersState(enabled_uniform_buffer_masks, &uniform_buffer_sizes);

        buffer_cache.UnbindGraphicsStorageBuffers(stage);
        size_t ssbo_index{};
        for (const auto& desc : info.storage_buffers_descriptors) {
            ASSERT(desc.count == 1);
            buffer_cache.BindGraphicsStorageBuffer(stage, ssbo_index, desc.cbuf_index,
                                                   desc.cbuf_offset, desc.is_written);
            ++ssbo_index;
        }
        const auto& cbufs{maxwell3d->state.shader_stages[stage].const_buffers};
        const auto read_handle{[&](const auto& desc, u32 index) {
            ASSERT(cbufs[desc.cbuf_index].enabled);
            const u32 index_offset{index << desc.size_shift};
            const u32 offset{desc.cbuf_offset + index_offset};
            const GPUVAddr addr{cbufs[desc.cbuf_index].address + offset};
            if constexpr (std::is_same_v<decltype(desc), const Shader::TextureDescriptor&> ||
                          std::is_same_v<decltype(desc), const Shader::TextureBufferDescriptor&>) {
                if (desc.has_secondary) {
                    ASSERT(cbufs[desc.secondary_cbuf_index].enabled);
                    const u32 second_offset{desc.secondary_cbuf_offset + index_offset};
                    const GPUVAddr separate_addr{cbufs[desc.secondary_cbuf_index].address +
                                                 second_offset};
                    const u32 lhs_raw{gpu_memory->Read<u32>(addr) << desc.shift_left};
                    const u32 rhs_raw{gpu_memory->Read<u32>(separate_addr)
                                      << desc.secondary_shift_left};
                    const u32 raw{lhs_raw | rhs_raw};
                    return TexturePair(raw, via_header_index);
                }
            }
            auto a = gpu_memory->Read<u32>(addr);
            // HACK: this particular texture breaks SMO
            if (a == 310378931)
                a = 310378932;

            return TexturePair(a, via_header_index);
        }};
        const auto add_image{[&](const auto& desc, bool blacklist) {
            for (u32 index = 0; index < desc.count; ++index) {
                const auto handle{read_handle(desc, index)};
                views[view_index++] = {
                    .index = handle.first,
                    .blacklist = blacklist,
                    .id = {},
                };
            }
        }};
        /*
        if constexpr (Spec::has_texture_buffers) {
            for (const auto& desc : info.texture_buffer_descriptors) {
                add_image(desc, false);
            }
        }
        */
        /*
        if constexpr (Spec::has_image_buffers) {
            for (const auto& desc : info.image_buffer_descriptors) {
                add_image(desc, false);
            }
        }
        */
        for (const auto& desc : info.texture_descriptors) {
            for (u32 index = 0; index < desc.count; ++index) {
                const auto handle{read_handle(desc, index)};
                views[view_index++] = {handle.first};

                VideoCommon::SamplerId sampler{texture_cache.GetGraphicsSamplerId(handle.second)};
                samplers[sampler_index++] = sampler;
            }
        }
        for (const auto& desc : info.image_descriptors) {
            add_image(desc, desc.is_written);
        }
    }};

    configure_stage(0);
    configure_stage(4);

    buffer_cache.UpdateGraphicsBuffers(is_indexed);
    buffer_cache.BindHostGeometryBuffers(is_indexed);

    // Bind resources
    const auto bind_stage_resources{[&](u32 stage) {
        std::array<VideoCommon::ImageViewInOut, 32>& views{all_views[stage].views};
        std::array<VideoCommon::SamplerId, 32>& samplers{all_samplers[stage].samplers};
        const size_t& view_index{all_views[stage].view_index};
        const size_t& sampler_index{all_samplers[stage].sampler_index};

        // Buffers
        buffer_cache.BindHostStageBuffers(stage);

        // Textures
        texture_cache.FillGraphicsImageViews<true>(std::span(views.data(), view_index));
        for (u8 i = 0; i < view_index; i++) {
            const VideoCommon::ImageViewInOut& view{views[i]};
            ImageView& image_view{texture_cache.GetImageView(view.id)};
            command_recorder.SetTexture(stage, image_view.GetHandle(), i);
        }

        // Samplers
        for (u8 i = 0; i < sampler_index; i++) {
            const VideoCommon::SamplerId& sampler_id{samplers[i]};
            Sampler& sampler{texture_cache.GetSampler(sampler_id)};
            command_recorder.SetSamplerState(stage, sampler.GetHandle(), i);
        }
    }};

    bind_stage_resources(0);
    bind_stage_resources(4);

    // Begin render pass
    texture_cache.UpdateRenderTargets(false);
    const Framebuffer* const framebuffer = texture_cache.GetFramebuffer();
    if (!framebuffer) {
        return;
    }
    command_recorder.BeginOrContinueRenderPass(framebuffer->GetHandle());

    command_recorder.SetRenderPipelineState(pipeline_state);
}

void GraphicsPipeline::MakePipeline(MTL::RenderPassDescriptor* render_pass) {
    MTL::RenderPipelineDescriptor* pipeline_descriptor =
        MTL::RenderPipelineDescriptor::alloc()->init();
    pipeline_descriptor->setVertexFunction(functions[0]);
    pipeline_descriptor->setFragmentFunction(functions[4]);
    // pipeline_descriptor->setVertexDescriptor(vertex_descriptor);
    // TODO: get the attachment count from render pass descriptor
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
