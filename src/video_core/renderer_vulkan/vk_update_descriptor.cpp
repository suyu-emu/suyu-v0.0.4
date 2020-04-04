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
    entries.clear();
}

void VKUpdateDescriptorQueue::Send(VkDescriptorUpdateTemplateKHR update_template,
                                   VkDescriptorSet set) {
    if (payload.size() + entries.size() >= payload.max_size()) {
        LOG_WARNING(Render_Vulkan, "Payload overflow, waiting for worker thread");
        scheduler.WaitWorker();
        payload.clear();
    }

    // TODO(Rodrigo): Rework to write the payload directly
    const auto payload_start = payload.data() + payload.size();
    for (const auto& entry : entries) {
        if (const auto image = std::get_if<VkDescriptorImageInfo>(&entry)) {
            payload.push_back(*image);
        } else if (const auto buffer = std::get_if<VkDescriptorBufferInfo>(&entry)) {
            payload.push_back(*buffer);
        } else if (const auto texel = std::get_if<VkBufferView>(&entry)) {
            payload.push_back(*texel);
        } else {
            UNREACHABLE();
        }
    }

    scheduler.Record(
        [payload_start, set, update_template, logical = &device.GetLogical()](vk::CommandBuffer) {
            logical->UpdateDescriptorSet(set, update_template, payload_start);
        });
}

} // namespace Vulkan
