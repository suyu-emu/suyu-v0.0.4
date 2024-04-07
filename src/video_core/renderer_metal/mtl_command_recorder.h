// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

namespace Metal {

class Device;

enum class EncoderType { Render, Compute, Blit };

class CommandRecorder {
public:
    CommandRecorder(const Device& device_);
    ~CommandRecorder();

    void BeginRenderPass(MTL::RenderPassDescriptor* render_pass_descriptor);

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
    MTL::CommandBuffer* command_buffer = nil;
    MTL::CommandEncoder* encoder = nil;

    EncoderType encoder_type;

    void RequireCommandBuffer();
};

} // namespace Metal
