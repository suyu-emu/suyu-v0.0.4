// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/readable_event.h"
#include "core/hle/kernel/writable_event.h"
#include "core/hle/service/nvflinger/buffer_queue.h"

namespace Service::NVFlinger {

BufferQueue::BufferQueue(Kernel::KernelCore& kernel, u32 id, u64 layer_id)
    : id(id), layer_id(layer_id) {
    buffer_wait_event = Kernel::WritableEvent::CreateEventPair(kernel, "BufferQueue NativeHandle");
}

BufferQueue::~BufferQueue() = default;

void BufferQueue::SetPreallocatedBuffer(u32 slot, const IGBPBuffer& igbp_buffer) {
    LOG_WARNING(Service, "Adding graphics buffer {}", slot);

    free_buffers.push_back(slot);
    queue.push_back({
        .slot = slot,
        .status = Buffer::Status::Free,
        .igbp_buffer = igbp_buffer,
        .transform = {},
        .crop_rect = {},
        .swap_interval = 0,
        .multi_fence = {},
    });

    buffer_wait_event.writable->Signal();
}

std::optional<std::pair<u32, Service::Nvidia::MultiFence*>> BufferQueue::DequeueBuffer(u32 width,
                                                                                       u32 height) {

    if (free_buffers.empty()) {
        return std::nullopt;
    }

    auto f_itr = free_buffers.begin();
    auto itr = queue.end();

    while (f_itr != free_buffers.end()) {
        auto slot = *f_itr;
        itr = std::find_if(queue.begin(), queue.end(), [&](const Buffer& buffer) {
            // Only consider free buffers. Buffers become free once again after they've been
            // Acquired and Released by the compositor, see the NVFlinger::Compose method.
            if (buffer.status != Buffer::Status::Free) {
                return false;
            }

            if (buffer.slot != slot) {
                return false;
            }

            // Make sure that the parameters match.
            return buffer.igbp_buffer.width == width && buffer.igbp_buffer.height == height;
        });

        if (itr != queue.end()) {
            free_buffers.erase(f_itr);
            break;
        }
        ++f_itr;
    }

    if (itr == queue.end()) {
        return std::nullopt;
    }

    itr->status = Buffer::Status::Dequeued;
    return {{itr->slot, &itr->multi_fence}};
}

const IGBPBuffer& BufferQueue::RequestBuffer(u32 slot) const {
    auto itr = std::find_if(queue.begin(), queue.end(),
                            [&](const Buffer& buffer) { return buffer.slot == slot; });
    ASSERT(itr != queue.end());
    ASSERT(itr->status == Buffer::Status::Dequeued);
    return itr->igbp_buffer;
}

void BufferQueue::QueueBuffer(u32 slot, BufferTransformFlags transform,
                              const Common::Rectangle<int>& crop_rect, u32 swap_interval,
                              Service::Nvidia::MultiFence& multi_fence) {
    auto itr = std::find_if(queue.begin(), queue.end(),
                            [&](const Buffer& buffer) { return buffer.slot == slot; });
    ASSERT(itr != queue.end());
    ASSERT(itr->status == Buffer::Status::Dequeued);
    itr->status = Buffer::Status::Queued;
    itr->transform = transform;
    itr->crop_rect = crop_rect;
    itr->swap_interval = swap_interval;
    itr->multi_fence = multi_fence;
    queue_sequence.push_back(slot);
}

void BufferQueue::CancelBuffer(u32 slot, const Service::Nvidia::MultiFence& multi_fence) {
    const auto itr = std::find_if(queue.begin(), queue.end(),
                                  [slot](const Buffer& buffer) { return buffer.slot == slot; });
    ASSERT(itr != queue.end());
    ASSERT(itr->status != Buffer::Status::Free);
    itr->status = Buffer::Status::Free;
    itr->multi_fence = multi_fence;
    itr->swap_interval = 0;

    free_buffers.push_back(slot);

    buffer_wait_event.writable->Signal();
}

std::optional<std::reference_wrapper<const BufferQueue::Buffer>> BufferQueue::AcquireBuffer() {
    auto itr = queue.end();
    // Iterate to find a queued buffer matching the requested slot.
    while (itr == queue.end() && !queue_sequence.empty()) {
        const u32 slot = queue_sequence.front();
        itr = std::find_if(queue.begin(), queue.end(), [&slot](const Buffer& buffer) {
            return buffer.status == Buffer::Status::Queued && buffer.slot == slot;
        });
        queue_sequence.pop_front();
    }
    if (itr == queue.end()) {
        return std::nullopt;
    }
    itr->status = Buffer::Status::Acquired;
    return *itr;
}

void BufferQueue::ReleaseBuffer(u32 slot) {
    auto itr = std::find_if(queue.begin(), queue.end(),
                            [&](const Buffer& buffer) { return buffer.slot == slot; });
    ASSERT(itr != queue.end());
    ASSERT(itr->status == Buffer::Status::Acquired);
    itr->status = Buffer::Status::Free;
    free_buffers.push_back(slot);

    buffer_wait_event.writable->Signal();
}

void BufferQueue::Disconnect() {
    queue.clear();
    queue_sequence.clear();
    id = 1;
    layer_id = 1;
}

u32 BufferQueue::Query(QueryType type) {
    LOG_WARNING(Service, "(STUBBED) called type={}", type);

    switch (type) {
    case QueryType::NativeWindowFormat:
        return static_cast<u32>(PixelFormat::RGBA8888);
    }

    UNIMPLEMENTED();
    return 0;
}

std::shared_ptr<Kernel::WritableEvent> BufferQueue::GetWritableBufferWaitEvent() const {
    return buffer_wait_event.writable;
}

std::shared_ptr<Kernel::ReadableEvent> BufferQueue::GetBufferWaitEvent() const {
    return buffer_wait_event.readable;
}

} // namespace Service::NVFlinger
