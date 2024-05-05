// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "video_core/renderer_metal/mtl_command_recorder.h"
#include "video_core/renderer_metal/mtl_device.h"

namespace Metal {

CommandRecorder::CommandRecorder(const Device& device_) : device(device_) {}

CommandRecorder::~CommandRecorder() = default;

void CommandRecorder::BeginOrContinueRenderPass(MTL::RenderPassDescriptor* render_pass) {
    bool should_reset_bound_resources = false;
    if (render_pass != render_state.render_pass) {
        EndEncoding();
        RequireCommandBuffer();
        encoder = command_buffer->renderCommandEncoder(render_pass);
        encoder_type = EncoderType::Render;
        render_state.render_pass = render_pass;
        should_reset_bound_resources = true;
    }
    const auto bind_resources{[&](size_t stage) {
        // Buffers
        for (u8 i = 0; i < MAX_BUFFERS; i++) {
            auto& bound_buffer = render_state.buffers[stage][i];
            if (bound_buffer.buffer &&
                (bound_buffer.needs_update || should_reset_bound_resources)) {
                switch (stage) {
                case 0:
                    GetRenderCommandEncoderUnchecked()->setVertexBuffer(bound_buffer.buffer, i,
                                                                        bound_buffer.offset);
                    break;
                case 4:
                    GetRenderCommandEncoderUnchecked()->setFragmentBuffer(bound_buffer.buffer, i,
                                                                          bound_buffer.offset);
                    break;
                }
                bound_buffer.needs_update = false;
            }
        }
        // Textures
        for (u8 i = 0; i < MAX_TEXTURES; i++) {
            auto& bound_texture = render_state.textures[stage][i];
            if (bound_texture.texture &&
                (bound_texture.needs_update || should_reset_bound_resources)) {
                switch (stage) {
                case 0:
                    GetRenderCommandEncoderUnchecked()->setVertexTexture(bound_texture.texture, i);
                    break;
                case 4:
                    GetRenderCommandEncoderUnchecked()->setFragmentTexture(bound_texture.texture,
                                                                           i);
                    break;
                }
                bound_texture.needs_update = false;
            }
        }
        // Sampler states
        for (u8 i = 0; i < MAX_SAMPLERS; i++) {
            auto& bound_sampler_state = render_state.sampler_states[stage][i];
            if (bound_sampler_state.sampler_state &&
                (bound_sampler_state.needs_update || should_reset_bound_resources)) {
                switch (stage) {
                case 0:
                    GetRenderCommandEncoderUnchecked()->setVertexSamplerState(
                        bound_sampler_state.sampler_state, i);
                    break;
                case 4:
                    GetRenderCommandEncoderUnchecked()->setFragmentSamplerState(
                        bound_sampler_state.sampler_state, i);
                    break;
                }
                bound_sampler_state.needs_update = false;
            }
        }
    }};

    bind_resources(0);
    bind_resources(4);

    if (should_reset_bound_resources) {
        for (size_t stage = 0; stage < 5; stage++) {
            for (u8 i = 0; i < MAX_BUFFERS; i++) {
                render_state.buffers[stage][i].buffer = nullptr;
            }
            for (u8 i = 0; i < MAX_TEXTURES; i++) {
                render_state.textures[stage][i].texture = nullptr;
            }
            for (u8 i = 0; i < MAX_SAMPLERS; i++) {
                render_state.sampler_states[stage][i].sampler_state = nullptr;
            }
        }
    }
}

void CommandRecorder::RequireComputeEncoder() {
    RequireCommandBuffer();
    if (!encoder || encoder_type != EncoderType::Compute) {
        EndEncoding();
        encoder = command_buffer->computeCommandEncoder();
        encoder_type = EncoderType::Compute;
    }
}

void CommandRecorder::RequireBlitEncoder() {
    RequireCommandBuffer();
    if (!encoder || encoder_type != EncoderType::Blit) {
        EndEncoding();
        encoder = command_buffer->blitCommandEncoder();
        encoder_type = EncoderType::Blit;
    }
}

void CommandRecorder::EndEncoding() {
    if (encoder) {
        encoder->endEncoding();
        //[encoder release];
        encoder = nullptr;
        if (encoder_type == EncoderType::Render) {
            render_state.render_pass = nullptr;
            render_state.pipeline_state = nullptr;
        }
    }
}

void CommandRecorder::Present(CA::MetalDrawable* drawable) {
    EndEncoding();
    command_buffer->presentDrawable(drawable);
}

void CommandRecorder::Submit() {
    if (command_buffer) {
        EndEncoding();
        command_buffer->commit();
        //[command_buffer release];
        command_buffer = nullptr;
    }
}

void CommandRecorder::RequireCommandBuffer() {
    if (!command_buffer) {
        command_buffer = device.GetCommandQueue()->commandBuffer();
    }
}

} // namespace Metal
