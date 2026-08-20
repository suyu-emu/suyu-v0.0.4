// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <optional>

#include "video_core/renderer_vulkan/vk_master_semaphore.h"
#include "video_core/renderer_vulkan/vk_resource_pool.h"

namespace Vulkan {

ResourcePool::ResourcePool(MasterSemaphore& master_semaphore_, size_t grow_step_)
    : master_semaphore{&master_semaphore_}, grow_step{grow_step_} {}

size_t ResourcePool::CommitResource() {
    const auto search = [this](size_t begin, size_t end, u64 gpu_tick) -> std::optional<size_t> {
        for (size_t iterator = begin; iterator < end; ++iterator) {
            if (gpu_tick >= ticks[iterator]) {
                ticks[iterator] = master_semaphore->CurrentTick();
                return iterator;
            }
        }
        return std::nullopt;
    };
    const auto find_free = [&](u64 gpu_tick) -> std::optional<size_t> {
        std::optional<size_t> result = search(hint_iterator, ticks.size(), gpu_tick);
        if (!result) {
            result = search(0, hint_iterator, gpu_tick);
        }
        return result;
    };
    std::optional<size_t> found = find_free(master_semaphore->KnownGpuTick());
    if (!found) {
        master_semaphore->Refresh();
        found = find_free(master_semaphore->KnownGpuTick());
        if (!found) {
            // Both searches failed, the pool is full; handle it.
            const size_t free_resource = ManageOverflow();

            ticks[free_resource] = master_semaphore->CurrentTick();
            found = free_resource;
        }
    }
    // Free iterator is hinted to the resource after the one that's been committed.
    hint_iterator = (*found + 1) % ticks.size();
    return *found;
}

size_t ResourcePool::ManageOverflow() {
    const size_t old_capacity = ticks.size();
    Grow();

    // The last entry is guaranteed to be free, since it's the first element of the freshly
    // allocated resources.
    return old_capacity;
}

void ResourcePool::Grow() {
    const size_t old_capacity = ticks.size();
    ticks.resize(old_capacity + grow_step);
    Allocate(old_capacity, old_capacity + grow_step);
}

} // namespace Vulkan
