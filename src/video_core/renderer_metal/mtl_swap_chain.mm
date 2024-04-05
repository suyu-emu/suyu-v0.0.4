// SPDX-License-Identifier: GPL-2.0-or-later

#include "video_core/renderer_metal/mtl_device.h"
#include "video_core/renderer_metal/mtl_swap_chain.h"

namespace Metal {

SwapChain::SwapChain(const Device& device_, const CAMetalLayer* layer_) : device(device_), layer([layer_ retain]) {
    // Give the layer our device
    layer.device = device.GetDevice();
}

SwapChain::~SwapChain() {
    [layer release];
}

void SwapChain::AcquireNextDrawable() {
    // Get the next drawable
    drawable = [layer nextDrawable];
}

void SwapChain::Present(MTLCommandBuffer_t command_buffer) {
    [command_buffer presentDrawable:drawable];
}

MTLTexture_t SwapChain::GetDrawableTexture() {
    return drawable.texture;
}

} // namespace Metal
