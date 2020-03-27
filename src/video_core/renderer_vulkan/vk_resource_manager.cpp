// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <optional>
#include "common/assert.h"
#include "common/logging/log.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_resource_manager.h"
#include "video_core/renderer_vulkan/wrapper.h"

namespace Vulkan {

namespace {

// TODO(Rodrigo): Fine tune these numbers.
constexpr std::size_t COMMAND_BUFFER_POOL_SIZE = 0x1000;
constexpr std::size_t FENCES_GROW_STEP = 0x40;

VkFenceCreateInfo BuildFenceCreateInfo() {
    VkFenceCreateInfo fence_ci;
    fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_ci.pNext = nullptr;
    fence_ci.flags = 0;
    return fence_ci;
}

} // Anonymous namespace

class CommandBufferPool final : public VKFencedPool {
public:
    CommandBufferPool(const VKDevice& device)
        : VKFencedPool(COMMAND_BUFFER_POOL_SIZE), device{device} {}

    void Allocate(std::size_t begin, std::size_t end) override {
        // Command buffers are going to be commited, recorded, executed every single usage cycle.
        // They are also going to be reseted when commited.
        VkCommandPoolCreateInfo command_pool_ci;
        command_pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        command_pool_ci.pNext = nullptr;
        command_pool_ci.flags =
            VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        command_pool_ci.queueFamilyIndex = device.GetGraphicsFamily();

        Pool& pool = pools.emplace_back();
        pool.handle = device.GetLogical().CreateCommandPool(command_pool_ci);
        pool.cmdbufs = pool.handle.Allocate(COMMAND_BUFFER_POOL_SIZE);
    }

    VkCommandBuffer Commit(VKFence& fence) {
        const std::size_t index = CommitResource(fence);
        const auto pool_index = index / COMMAND_BUFFER_POOL_SIZE;
        const auto sub_index = index % COMMAND_BUFFER_POOL_SIZE;
        return pools[pool_index].cmdbufs[sub_index];
    }

private:
    struct Pool {
        vk::CommandPool handle;
        vk::CommandBuffers cmdbufs;
    };

    const VKDevice& device;
    std::vector<Pool> pools;
};

VKResource::VKResource() = default;

VKResource::~VKResource() = default;

VKFence::VKFence(const VKDevice& device)
    : device{device}, handle{device.GetLogical().CreateFence(BuildFenceCreateInfo())} {}

VKFence::~VKFence() = default;

void VKFence::Wait() {
    switch (const VkResult result = handle.Wait()) {
    case VK_SUCCESS:
        return;
    case VK_ERROR_DEVICE_LOST:
        device.ReportLoss();
        [[fallthrough]];
    default:
        throw vk::Exception(result);
    }
}

void VKFence::Release() {
    ASSERT(is_owned);
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

    if (gpu_wait) {
        // Wait for the fence if it has been requested.
        (void)handle.Wait();
    } else {
        if (handle.GetStatus() != VK_SUCCESS) {
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
    handle.Reset();
    is_used = false;
    return true;
}

void VKFence::Protect(VKResource* resource) {
    protected_resources.push_back(resource);
}

void VKFence::Unprotect(VKResource* resource) {
    const auto it = std::find(protected_resources.begin(), protected_resources.end(), resource);
    ASSERT(it != protected_resources.end());

    resource->OnFenceRemoval(this);
    protected_resources.erase(it);
}

void VKFence::RedirectProtection(VKResource* old_resource, VKResource* new_resource) noexcept {
    std::replace(std::begin(protected_resources), std::end(protected_resources), old_resource,
                 new_resource);
}

VKFenceWatch::VKFenceWatch() = default;

VKFenceWatch::VKFenceWatch(VKFence& initial_fence) {
    Watch(initial_fence);
}

VKFenceWatch::VKFenceWatch(VKFenceWatch&& rhs) noexcept {
    fence = std::exchange(rhs.fence, nullptr);
    if (fence) {
        fence->RedirectProtection(&rhs, this);
    }
}

VKFenceWatch& VKFenceWatch::operator=(VKFenceWatch&& rhs) noexcept {
    fence = std::exchange(rhs.fence, nullptr);
    if (fence) {
        fence->RedirectProtection(&rhs, this);
    }
    return *this;
}

VKFenceWatch::~VKFenceWatch() {
    if (fence) {
        fence->Unprotect(this);
    }
}

void VKFenceWatch::Wait() {
    if (fence == nullptr) {
        return;
    }
    fence->Wait();
    fence->Unprotect(this);
}

void VKFenceWatch::Watch(VKFence& new_fence) {
    Wait();
    fence = &new_fence;
    fence->Protect(this);
}

bool VKFenceWatch::TryWatch(VKFence& new_fence) {
    if (fence) {
        return false;
    }
    fence = &new_fence;
    fence->Protect(this);
    return true;
}

void VKFenceWatch::OnFenceRemoval(VKFence* signaling_fence) {
    ASSERT_MSG(signaling_fence == fence, "Removing the wrong fence");
    fence = nullptr;
}

VKFencedPool::VKFencedPool(std::size_t grow_step) : grow_step{grow_step} {}

VKFencedPool::~VKFencedPool() = default;

std::size_t VKFencedPool::CommitResource(VKFence& fence) {
    const auto Search = [&](std::size_t begin, std::size_t end) -> std::optional<std::size_t> {
        for (std::size_t iterator = begin; iterator < end; ++iterator) {
            if (watches[iterator]->TryWatch(fence)) {
                // The resource is now being watched, a free resource was successfully found.
                return iterator;
            }
        }
        return {};
    };
    // Try to find a free resource from the hinted position to the end.
    auto found = Search(free_iterator, watches.size());
    if (!found) {
        // Search from beginning to the hinted position.
        found = Search(0, free_iterator);
        if (!found) {
            // Both searches failed, the pool is full; handle it.
            const std::size_t free_resource = ManageOverflow();

            // Watch will wait for the resource to be free.
            watches[free_resource]->Watch(fence);
            found = free_resource;
        }
    }
    // Free iterator is hinted to the resource after the one that's been commited.
    free_iterator = (*found + 1) % watches.size();
    return *found;
}

std::size_t VKFencedPool::ManageOverflow() {
    const std::size_t old_capacity = watches.size();
    Grow();

    // The last entry is guaranted to be free, since it's the first element of the freshly
    // allocated resources.
    return old_capacity;
}

void VKFencedPool::Grow() {
    const std::size_t old_capacity = watches.size();
    watches.resize(old_capacity + grow_step);
    std::generate(watches.begin() + old_capacity, watches.end(),
                  []() { return std::make_unique<VKFenceWatch>(); });
    Allocate(old_capacity, old_capacity + grow_step);
}

VKResourceManager::VKResourceManager(const VKDevice& device) : device{device} {
    GrowFences(FENCES_GROW_STEP);
    command_buffer_pool = std::make_unique<CommandBufferPool>(device);
}

VKResourceManager::~VKResourceManager() = default;

VKFence& VKResourceManager::CommitFence() {
    const auto StepFences = [&](bool gpu_wait, bool owner_wait) -> VKFence* {
        const auto Tick = [=](auto& fence) { return fence->Tick(gpu_wait, owner_wait); };
        const auto hinted = fences.begin() + fences_iterator;

        auto it = std::find_if(hinted, fences.end(), Tick);
        if (it == fences.end()) {
            it = std::find_if(fences.begin(), hinted, Tick);
            if (it == hinted) {
                return nullptr;
            }
        }
        fences_iterator = std::distance(fences.begin(), it) + 1;
        if (fences_iterator >= fences.size())
            fences_iterator = 0;

        auto& fence = *it;
        fence->Commit();
        return fence.get();
    };

    VKFence* found_fence = StepFences(false, false);
    if (!found_fence) {
        // Try again, this time waiting.
        found_fence = StepFences(true, false);

        if (!found_fence) {
            // Allocate new fences and try again.
            LOG_INFO(Render_Vulkan, "Allocating new fences {} -> {}", fences.size(),
                     fences.size() + FENCES_GROW_STEP);

            GrowFences(FENCES_GROW_STEP);
            found_fence = StepFences(true, false);
            ASSERT(found_fence != nullptr);
        }
    }
    return *found_fence;
}

VkCommandBuffer VKResourceManager::CommitCommandBuffer(VKFence& fence) {
    return command_buffer_pool->Commit(fence);
}

void VKResourceManager::GrowFences(std::size_t new_fences_count) {
    const std::size_t previous_size = fences.size();
    fences.resize(previous_size + new_fences_count);

    std::generate(fences.begin() + previous_size, fences.end(),
                  [this] { return std::make_unique<VKFence>(device); });
}

} // namespace Vulkan
