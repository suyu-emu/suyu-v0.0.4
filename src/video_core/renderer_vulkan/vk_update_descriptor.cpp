// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <variant>
#include <boost/container/static_vector.hpp>

#include "common/assert.h"
#include "common/logging/log.h"
#include "video_core/renderer_vulkan/declarations.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_update_descriptor.h"

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

void VKUpdateDescriptorQueue::Send(vk::DescriptorUpdateTemplate update_template,
                                   vk::DescriptorSet set) {
    if (payload.size() + entries.size() >= payload.max_size()) {
        LOG_WARNING(Render_Vulkan, "Payload overflow, waiting for worker thread");
        scheduler.WaitWorker();
        payload.clear();
    }

    const auto payload_start = payload.data() + payload.size();
    for (const auto& entry : entries) {
        if (const auto image = std::get_if<vk::DescriptorImageInfo>(&entry)) {
            payload.push_back(*image);
        } else if (const auto buffer = std::get_if<Buffer>(&entry)) {
            payload.emplace_back(*buffer->buffer, buffer->offset, buffer->size);
        } else if (const auto texel = std::get_if<vk::BufferView>(&entry)) {
            payload.push_back(*texel);
        } else {
            UNREACHABLE();
        }
    }

    scheduler.Record([dev = device.GetLogical(), payload_start, set,
                      update_template]([[maybe_unused]] auto cmdbuf, auto& dld) {
        dev.updateDescriptorSetWithTemplate(set, update_template, payload_start, dld);
    });
}

} // namespace Vulkan
