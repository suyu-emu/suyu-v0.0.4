// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "video_core/renderer_metal/objc_bridge.h"

namespace Metal {

class Device;

class SwapChain {
public:
    SwapChain(const Device& device_, const CAMetalLayer* layer_);
    ~SwapChain();

    void AcquireNextDrawable();

    void Present(MTLCommandBuffer_t command_buffer);

    MTLTexture_t GetDrawableTexture();

private:
    const Device& device;
    const CAMetalLayer* layer;

    CAMetalDrawable_t drawable;
};

} // namespace Metal
