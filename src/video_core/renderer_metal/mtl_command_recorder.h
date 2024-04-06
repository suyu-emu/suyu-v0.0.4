// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "objc_bridge.h"
#include "video_core/renderer_metal/objc_bridge.h"

namespace Metal {

class Device;

enum class EncoderType { Render, Compute, Blit };

class CommandRecorder {
public:
    CommandRecorder(const Device& device_);
    ~CommandRecorder();

    void BeginRenderPass(MTLRenderPassDescriptor* render_pass_descriptor);

    void CheckIfRenderPassIsActive() {
        if (!encoder || encoder_type != EncoderType::Render) {
            throw std::runtime_error(
                "Trying to perform render command, but render pass is not active");
        }
    }

    void RequireComputeEncoder();

    void RequireBlitEncoder();

    void EndEncoding();

    void Present(CAMetalDrawable_t drawable);

    void Submit();

    MTLCommandBuffer_t GetCommandBuffer() {
        return command_buffer;
    }

    MTLCommandEncoder_t GetCommandEncoder() {
        return encoder;
    }

private:
    const Device& device;

    MTLCommandBuffer_t command_buffer = nil;
    MTLCommandEncoder_t encoder = nil;

    EncoderType encoder_type;

    void RequireCommandBuffer();
};

} // namespace Metal
