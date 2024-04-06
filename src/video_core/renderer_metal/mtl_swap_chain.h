// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "video_core/renderer_metal/objc_bridge.h"

namespace Metal {

class Device;
class CommandRecorder;

class SwapChain {
public:
    SwapChain(const Device& device_, CommandRecorder& command_recorder_,
              const CAMetalLayer* layer_);
    ~SwapChain();

    void AcquireNextDrawable();

    void Present();

    // Can only be called between AcquireNextDrawable and Present
    MTLTexture_t GetDrawableTexture();

private:
    const Device& device;
    CommandRecorder& command_recorder;
    const CAMetalLayer* layer;

    CAMetalDrawable_t drawable = nil;
};

} // namespace Metal
