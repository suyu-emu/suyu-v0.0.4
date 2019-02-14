// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include "video_core/renderer_vulkan/declarations.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_resource_manager.h"

namespace Vulkan {

VKResource::VKResource() = default;

VKResource::~VKResource() = default;

VKFence::VKFence(const VKDevice& device, UniqueFence handle)
    : device{device}, handle{std::move(handle)} {}

VKFence::~VKFence() = default;

void VKFence::Wait() {
    const auto dev = device.GetLogical();
    const auto& dld = device.GetDispatchLoader();
    dev.waitForFences({*handle}, true, std::numeric_limits<u64>::max(), dld);
}

void VKFence::Release() {
    is_owned = false;
}

void VKFence::Commit() {
    is_owned = true;
    is_used = true;
}

bool VKFence::Tick(bool gpu_wait, bool owner_wait) {
    if (!is_used) {
        // If a fence is not used it's always free.
        return true;
    }
    if (is_owned && !owner_wait) {
        // The fence is still being owned (Release has not been called) and ownership wait has
        // not been asked.
        return false;
    }

    const auto dev = device.GetLogical();
    const auto& dld = device.GetDispatchLoader();
    if (gpu_wait) {
        // Wait for the fence if it has been requested.
        dev.waitForFences({*handle}, true, std::numeric_limits<u64>::max(), dld);
    } else {
        if (dev.getFenceStatus(*handle, dld) != vk::Result::eSuccess) {
            // Vulkan fence is not ready, not much it can do here
            return false;
        }
    }

    // Broadcast resources their free state.
    for (auto* resource : protected_resources) {
        resource->OnFenceRemoval(this);
    }
    protected_resources.clear();

    // Prepare fence for reusage.
    dev.resetFences({*handle}, dld);
    is_used = false;
    return true;
}

void VKFence::Protect(VKResource* resource) {
    protected_resources.push_back(resource);
}

void VKFence::Unprotect(const VKResource* resource) {
    const auto it = std::find(protected_resources.begin(), protected_resources.end(), resource);
    if (it != protected_resources.end()) {
        protected_resources.erase(it);
    }
}

} // namespace Vulkan
