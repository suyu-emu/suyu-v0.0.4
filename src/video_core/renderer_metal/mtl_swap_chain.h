// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

namespace Metal {

class Device;
class CommandRecorder;

class SwapChain {
public:
    SwapChain(const Device& device_, CommandRecorder& command_recorder_, CA::MetalLayer* layer_);
    ~SwapChain();

    void AcquireNextDrawable();

    void Present();

    // Can only be called between AcquireNextDrawable and Present
    MTL::Texture* GetDrawableTexture();

private:
    const Device& device;
    CommandRecorder& command_recorder;
    CA::MetalLayer* layer;

    CA::MetalDrawable* drawable = nil;
};

} // namespace Metal
