// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "video_core/renderer_metal/mtl_command_recorder.h"
#include "video_core/renderer_metal/mtl_device.h"

#include <iostream>

namespace Metal {

CommandRecorder::CommandRecorder(const Device& device_) : device(device_) {}

CommandRecorder::~CommandRecorder() = default;

void CommandRecorder::BeginRenderPass(MTLRenderPassDescriptor* render_pass_descriptor) {
    RequireCommandBuffer();
    EndEncoding();
    encoder = [command_buffer renderCommandEncoderWithDescriptor:render_pass_descriptor];
    encoder_type = EncoderType::Render;
}

void CommandRecorder::RequireComputeEncoder() {
    RequireCommandBuffer();
    if (!encoder || encoder_type != EncoderType::Compute) {
        EndEncoding();
        encoder = [command_buffer computeCommandEncoder];
        encoder_type = EncoderType::Compute;
    }
}

void CommandRecorder::RequireBlitEncoder() {
    RequireCommandBuffer();
    if (!encoder || encoder_type != EncoderType::Blit) {
        EndEncoding();
        encoder = [command_buffer blitCommandEncoder];
        encoder_type = EncoderType::Blit;
    }
}

void CommandRecorder::EndEncoding() {
    if (encoder) {
        [encoder endEncoding];
        //[encoder release];
        encoder = nil;
    }
}

void CommandRecorder::Present(CAMetalDrawable_t drawable) {
    [command_buffer presentDrawable:drawable];
}

void CommandRecorder::Submit() {
    if (command_buffer) {
        EndEncoding();
        [command_buffer commit];
        //[command_buffer release];
        command_buffer = nil;
    }
}

void CommandRecorder::RequireCommandBuffer() {
    if (!command_buffer) {
        command_buffer = [device.GetCommandQueue() commandBuffer];
    }
}

} // namespace Metal
