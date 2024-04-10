// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

namespace Metal {

class Device;

enum class EncoderType { Render, Compute, Blit };

constexpr size_t MAX_BUFFERS = 31;
constexpr size_t MAX_TEXTURES = 31;
constexpr size_t MAX_SAMPLERS = 31;

struct RenderState {
    MTL::RenderPassDescriptor* render_pass{nullptr};
    MTL::RenderPipelineState* pipeline_state{nullptr};

    MTL::Buffer* vertex_buffers[MAX_BUFFERS] = {nullptr};
    MTL::Buffer* fragment_buffers[MAX_BUFFERS] = {nullptr};
    MTL::Buffer* compute_buffers[MAX_BUFFERS] = {nullptr};

    MTL::Texture* vertex_textures[MAX_TEXTURES] = {nullptr};
    MTL::Texture* fragment_textures[MAX_TEXTURES] = {nullptr};
    MTL::Texture* compute_textures[MAX_TEXTURES] = {nullptr};

    MTL::SamplerState* vertex_sampler_states[MAX_SAMPLERS] = {nullptr};
    MTL::SamplerState* fragment_sampler_states[MAX_SAMPLERS] = {nullptr};
    MTL::SamplerState* compute_sampler_states[MAX_SAMPLERS] = {nullptr};
};

// TODO: whenever a render pass gets interrupted by either a compute or blit command and application
// then tries to perform a render command, begin the same render pass, but with all load actions set
// to "load"
class CommandRecorder {
public:
    CommandRecorder(const Device& device_);
    ~CommandRecorder();

    void BeginOrContinueRenderPass(MTL::RenderPassDescriptor* render_pass);

    void CheckIfRenderPassIsActive() {
        if (!encoder || encoder_type != EncoderType::Render) {
            throw std::runtime_error(
                "Trying to perform render command, but render pass is not active");
        }
    }

    void RequireComputeEncoder();

    void RequireBlitEncoder();

    void EndEncoding();

    void Present(CA::MetalDrawable* drawable);

    void Submit();

    MTL::CommandBuffer* GetCommandBuffer() {
        return command_buffer;
    }

    MTL::RenderCommandEncoder* GetRenderCommandEncoder() {
        CheckIfRenderPassIsActive();

        return static_cast<MTL::RenderCommandEncoder*>(encoder);
    }

    MTL::ComputeCommandEncoder* GetComputeCommandEncoder() {
        RequireComputeEncoder();

        return static_cast<MTL::ComputeCommandEncoder*>(encoder);
    }

    MTL::BlitCommandEncoder* GetBlitCommandEncoder() {
        RequireBlitEncoder();

        return static_cast<MTL::BlitCommandEncoder*>(encoder);
    }

    // Render commands
    inline void SetRenderPipelineState(MTL::RenderPipelineState* pipeline_state) {
        if (pipeline_state != render_state.pipeline_state) {
            GetRenderCommandEncoder()->setRenderPipelineState(pipeline_state);
            render_state.pipeline_state = pipeline_state;
        }
    }

    inline void SetVertexBuffer(MTL::Buffer* buffer, size_t index) {
        if (buffer != render_state.vertex_buffers[index]) {
            GetRenderCommandEncoder()->setVertexBuffer(buffer, index, 0);
            render_state.vertex_buffers[index] = buffer;
        }
    }

    inline void SetFragmentBuffer(MTL::Buffer* buffer, size_t index) {
        if (buffer != render_state.fragment_buffers[index]) {
            GetRenderCommandEncoder()->setFragmentBuffer(buffer, index, 0);
            render_state.fragment_buffers[index] = buffer;
        }
    }

    inline void SetComputeBuffer(MTL::Buffer* buffer, size_t index) {
        if (buffer != render_state.compute_buffers[index]) {
            GetComputeCommandEncoder()->setBuffer(buffer, index, 0);
            render_state.compute_buffers[index] = buffer;
        }
    }

    inline void SetVertexTexture(MTL::Texture* texture, size_t index) {
        if (texture != render_state.vertex_textures[index]) {
            GetRenderCommandEncoder()->setVertexTexture(texture, index);
            render_state.vertex_textures[index] = texture;
        }
    }

    inline void SetFragmentTexture(MTL::Texture* texture, size_t index) {
        if (texture != render_state.fragment_textures[index]) {
            GetRenderCommandEncoder()->setFragmentTexture(texture, index);
            render_state.fragment_textures[index] = texture;
        }
    }

    inline void SetComputeTexture(MTL::Texture* texture, size_t index) {
        if (texture != render_state.compute_textures[index]) {
            GetComputeCommandEncoder()->setTexture(texture, index);
            render_state.compute_textures[index] = texture;
        }
    }

    inline void SetVertexSamplerState(MTL::SamplerState* sampler_state, size_t index) {
        if (sampler_state != render_state.vertex_sampler_states[index]) {
            GetRenderCommandEncoder()->setVertexSamplerState(sampler_state, index);
            render_state.vertex_sampler_states[index] = sampler_state;
        }
    }

    inline void SetFragmentSamplerState(MTL::SamplerState* sampler_state, size_t index) {
        if (sampler_state != render_state.fragment_sampler_states[index]) {
            GetRenderCommandEncoder()->setFragmentSamplerState(sampler_state, index);
            render_state.fragment_sampler_states[index] = sampler_state;
        }
    }

    inline void SetComputeSamplerState(MTL::SamplerState* sampler_state, size_t index) {
        if (sampler_state != render_state.compute_sampler_states[index]) {
            GetComputeCommandEncoder()->setSamplerState(sampler_state, index);
            render_state.compute_sampler_states[index] = sampler_state;
        }
    }

private:
    const Device& device;

    // HACK: Command buffers and encoders currently aren't released every frame due to Xcode
    // crashing in Debug mode. This leads to memory leaks
    MTL::CommandBuffer* command_buffer{nullptr};
    MTL::CommandEncoder* encoder{nullptr};

    EncoderType encoder_type;

    // Keep track of bound resources
    RenderState render_state{};

    void RequireCommandBuffer();
};

} // namespace Metal
