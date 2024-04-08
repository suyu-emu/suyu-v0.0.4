// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

namespace Metal {

class Device;

enum class EncoderType { Render, Compute, Blit };

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

private:
    const Device& device;

    // HACK: Command buffers and encoders currently aren't released every frame due to Xcode
    // crashing in Debug mode. This leads to memory leaks
    MTL::CommandBuffer* command_buffer{nullptr};
    MTL::CommandEncoder* encoder{nullptr};

    EncoderType encoder_type;

    // Keep track of last bound render pass
    MTL::RenderPassDescriptor* bound_render_pass{nullptr};

    void RequireCommandBuffer();
};

} // namespace Metal
