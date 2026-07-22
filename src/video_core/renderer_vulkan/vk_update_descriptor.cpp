// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <variant>
#include <boost/container/static_vector.hpp>

#include "common/assert.h"
#include "common/logging.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_update_descriptor.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

UpdateDescriptorQueue::UpdateDescriptorQueue(const Device& device_)
    : device{device_}
{
    payload_start = payload.data();
    payload_cursor = payload.data();
}

UpdateDescriptorQueue::~UpdateDescriptorQueue() = default;

void UpdateDescriptorQueue::TickFrame() {
    if (++frame_index >= FRAMES_IN_FLIGHT) {
        frame_index = 0;
    }
    payload_start = payload.data() + frame_index * FRAME_PAYLOAD_SIZE;
    payload_cursor = payload_start;
}

void UpdateDescriptorQueue::Acquire(Scheduler& scheduler, size_t required_entries) {
    static constexpr size_t DEFAULT_REQUIRED_ENTRIES = 0x400;
    const size_t reserve = required_entries > 0 ? required_entries : DEFAULT_REQUIRED_ENTRIES;
    ASSERT_MSG(reserve < FRAME_PAYLOAD_SIZE, "Descriptor reservation {} >= frame capacity {}",
               reserve, FRAME_PAYLOAD_SIZE);
    const size_t used = static_cast<size_t>(std::distance(payload_start, payload_cursor));
    if (used + reserve >= FRAME_PAYLOAD_SIZE) {
        LOG_WARNING(Render_Vulkan, "Payload overflow (used={}, reserve={}, capacity={})",
                    used, reserve, FRAME_PAYLOAD_SIZE);
        scheduler.WaitWorker();
        payload_cursor = payload_start;
    }
    upload_start = payload_cursor;
}

} // namespace Vulkan
