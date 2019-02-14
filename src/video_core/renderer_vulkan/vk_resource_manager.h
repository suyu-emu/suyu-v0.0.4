// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include "video_core/renderer_vulkan/declarations.h"

namespace Vulkan {

class VKDevice;
class VKFence;
class VKResourceManager;

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

/**
 * Fences take ownership of objects, protecting them from GPU-side or driver-side concurrent access.
 * They must be commited from the resource manager. Their usage flow is: commit the fence from the
 * resource manager, protect resources with it and use them, send the fence to an execution queue
 * and Wait for it if needed and then call Release. Used resources will automatically be signaled
 * when they are free to be reused.
 * @brief Protects resources for concurrent usage and signals its release.
 */
class VKFence {
    friend class VKResourceManager;

public:
    explicit VKFence(const VKDevice& device, UniqueFence handle);
    ~VKFence();

    /**
     * Waits for the fence to be signaled.
     * @warning You must have ownership of the fence and it has to be previously sent to a queue to
     * call this function.
     */
    void Wait();

    /**
     * Releases ownership of the fence. Pass after it has been sent to an execution queue.
     * Unmanaged usage of the fence after the call will result in undefined behavior because it may
     * be being used for something else.
     */
    void Release();

    /// Protects a resource with this fence.
    void Protect(VKResource* resource);

    /// Removes protection for a resource.
    void Unprotect(const VKResource* resource);

    /// Retreives the fence.
    operator vk::Fence() const {
        return *handle;
    }

private:
    /// Take ownership of the fence.
    void Commit();

    /**
     * Updates the fence status.
     * @warning Waiting for the owner might soft lock the execution.
     * @param gpu_wait Wait for the fence to be signaled by the driver.
     * @param owner_wait Wait for the owner to signal its freedom.
     * @returns True if the fence is free. Waiting for gpu and owner will always return true.
     */
    bool Tick(bool gpu_wait, bool owner_wait);

    const VKDevice& device;                       ///< Device handler
    UniqueFence handle;                           ///< Vulkan fence
    std::vector<VKResource*> protected_resources; ///< List of resources protected by this fence
    bool is_owned = false; ///< The fence has been commited but not released yet.
    bool is_used = false;  ///< The fence has been commited but it has not been checked to be free.
};

} // namespace Vulkan
