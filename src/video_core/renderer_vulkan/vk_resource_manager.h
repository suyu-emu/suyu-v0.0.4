// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "video_core/renderer_vulkan/declarations.h"

namespace Vulkan {

class VKFence;

/// Interface for a Vulkan resource
class VKResource {
public:
    explicit VKResource();
    virtual ~VKResource();

    /**
     * Signals the object that an owning fence has been signaled.
     * @param signaling_fence Fence that signals its usage end.
     */
    virtual void OnFenceRemoval(VKFence* signaling_fence) = 0;
};

} // namespace Vulkan
