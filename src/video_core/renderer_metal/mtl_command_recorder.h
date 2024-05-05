// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include "video_core/engines/maxwell_3d.h"

namespace Metal {

class Device;

enum class EncoderType { Render, Compute, Blit };

constexpr size_t MAX_BUFFERS = 31;
constexpr size_t MAX_TEXTURES = 31;
constexpr size_t MAX_SAMPLERS = 31;

struct BoundBuffer {
    bool needs_update{true};
    MTL::Buffer* buffer{nullptr};
    size_t offset{0};
};

struct BoundTexture {
    bool needs_update{true};
    MTL::Texture* texture{nullptr};
};

struct BoundSamplerState {
    bool needs_update{true};
    MTL::SamplerState* sampler_state{nullptr};
};

struct BoundIndexBuffer {
    MTL::Buffer* buffer{nullptr};
    size_t offset{0};
    MTL::IndexType index_format;
    MTL::PrimitiveType primitive_topology;
    u32 num_indices;
    u32 base_vertex;
};

struct RenderState {
    MTL::RenderPassDescriptor* render_pass{nullptr};
    MTL::RenderPipelineState* pipeline_state{nullptr};

    BoundBuffer buffers[5][MAX_BUFFERS] = {{}};
    BoundTexture textures[5][MAX_TEXTURES] = {{}};
    BoundSamplerState sampler_states[5][MAX_SAMPLERS] = {{}};
    BoundIndexBuffer bound_index_buffer;
};

// TODO: whenever a render pass gets interrupted by either a compute or blit command and application
// then tries to perform a render command, begin the same render pass, but with all load actions set
// to "load"
class CommandRecorder {
    using PrimitiveTopology = Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology;
    using IndexFormat = Tegra::Engines::Maxwell3D::Regs::IndexFormat;

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

    MTL::RenderCommandEncoder* GetRenderCommandEncoderUnchecked() {
        return static_cast<MTL::RenderCommandEncoder*>(encoder);
    }

    MTL::RenderCommandEncoder* GetRenderCommandEncoder() {
        CheckIfRenderPassIsActive();

        return GetRenderCommandEncoderUnchecked();
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

    inline void SetBuffer(size_t stage, MTL::Buffer* buffer, size_t index, size_t offset) {
        auto& bound_buffer = render_state.buffers[stage][index];
        if (buffer != bound_buffer.buffer) {
            bound_buffer = {true, buffer, offset};
        }
    }

    inline void SetTexture(size_t stage, MTL::Texture* texture, size_t index) {
        auto& bound_texture = render_state.textures[stage][index];
        if (texture != bound_texture.texture) {
            bound_texture = {true, texture};
        }
    }

    inline void SetSamplerState(size_t stage, MTL::SamplerState* sampler_state, size_t index) {
        auto& bound_sampler_state = render_state.sampler_states[stage][index];
        if (sampler_state != bound_sampler_state.sampler_state) {
            bound_sampler_state = {true, sampler_state};
        }
    }

    inline void SetIndexBuffer(MTL::Buffer* buffer, size_t offset, IndexFormat index_format,
                               PrimitiveTopology primitive_topology, u32 num_indices,
                               u32 base_vertex) {
        // TODO: convert parameters to Metal enums
        render_state.bound_index_buffer = {
            buffer,      offset,     MTL::IndexTypeUInt32, MTL::PrimitiveTypeTriangle,
            num_indices, base_vertex};
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
