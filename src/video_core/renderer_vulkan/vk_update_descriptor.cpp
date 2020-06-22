// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <variant>
#include <boost/container/static_vector.hpp>

#include "common/assert.h"
#include "common/logging/log.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_update_descriptor.h"
#include "video_core/renderer_vulkan/wrapper.h"

namespace Vulkan {

VKUpdateDescriptorQueue::VKUpdateDescriptorQueue(const VKDevice& device, VKScheduler& scheduler)
    : device{device}, scheduler{scheduler} {}

VKUpdateDescriptorQueue::~VKUpdateDescriptorQueue() = default;

void VKUpdateDescriptorQueue::TickFrame() {
    payload.clear();
}

void VKUpdateDescriptorQueue::Acquire() {
    // Minimum number of entries required.
    // This is the maximum number of entries a single draw call migth use.
    static constexpr std::size_t MIN_ENTRIES = 0x400;

    if (payload.size() + MIN_ENTRIES >= payload.max_size()) {
        LOG_WARNING(Render_Vulkan, "Payload overflow, waiting for worker thread");
        scheduler.WaitWorker();
        payload.clear();
    }
    upload_start = &*payload.end();
}

void VKUpdateDescriptorQueue::Send(VkDescriptorUpdateTemplateKHR update_template,
                                   VkDescriptorSet set) {
    const void* const data = upload_start;
    const vk::Device* const logical = &device.GetLogical();
    scheduler.Record([data, logical, set, update_template](vk::CommandBuffer) {
        logical->UpdateDescriptorSet(set, update_template, data);
    });
}

} // namespace Vulkan
