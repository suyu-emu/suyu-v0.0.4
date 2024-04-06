// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "video_core/renderer_metal/mtl_command_recorder.h"
#include "video_core/renderer_metal/mtl_device.h"
#include "video_core/renderer_metal/mtl_swap_chain.h"

namespace Metal {

SwapChain::SwapChain(const Device& device_, CommandRecorder& command_recorder_,
    const CAMetalLayer* layer_)
    : device(device_), command_recorder(command_recorder_), layer([layer_ retain]) {
    // Give the layer our device
    layer.device = device.GetDevice();
}

SwapChain::~SwapChain() {
    [layer release];
}

void SwapChain::AcquireNextDrawable() {
    // Get the next drawable
    drawable = [[layer nextDrawable] retain];
}

void SwapChain::Present() {
    command_recorder.Present(drawable);
    [drawable release];
}

MTLTexture_t SwapChain::GetDrawableTexture() {
    return drawable.texture;
}

} // namespace Metal
