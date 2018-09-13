// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/service/nvflinger/buffer_queue.h"

namespace Service::NVFlinger {

BufferQueue::BufferQueue(u32 id, u64 layer_id) : id(id), layer_id(layer_id) {
    auto& kernel = Core::System::GetInstance().Kernel();
    buffer_wait_event =
        Kernel::Event::Create(kernel, Kernel::ResetType::Sticky, "BufferQueue NativeHandle");
}

BufferQueue::~BufferQueue() = default;

void BufferQueue::SetPreallocatedBuffer(u32 slot, const IGBPBuffer& igbp_buffer) {
    Buffer buffer{};
    buffer.slot = slot;
    buffer.igbp_buffer = igbp_buffer;
    buffer.status = Buffer::Status::Free;

    LOG_WARNING(Service, "Adding graphics buffer {}", slot);

    queue.emplace_back(buffer);
    buffer_wait_event->Signal();
}

boost::optional<u32> BufferQueue::DequeueBuffer(u32 width, u32 height) {
    auto itr = std::find_if(queue.begin(), queue.end(), [&](const Buffer& buffer) {
        // Only consider free buffers. Buffers become free once again after they've been Acquired
        // and Released by the compositor, see the NVFlinger::Compose method.
        if (buffer.status != Buffer::Status::Free) {
            return false;
        }

        // Make sure that the parameters match.
        return buffer.igbp_buffer.width == width && buffer.igbp_buffer.height == height;
    });

    if (itr == queue.end()) {
        return boost::none;
    }

    itr->status = Buffer::Status::Dequeued;
    return itr->slot;
}

const IGBPBuffer& BufferQueue::RequestBuffer(u32 slot) const {
    auto itr = std::find_if(queue.begin(), queue.end(),
                            [&](const Buffer& buffer) { return buffer.slot == slot; });
    ASSERT(itr != queue.end());
    ASSERT(itr->status == Buffer::Status::Dequeued);
    return itr->igbp_buffer;
}

void BufferQueue::QueueBuffer(u32 slot, BufferTransformFlags transform,
                              const MathUtil::Rectangle<int>& crop_rect) {
    auto itr = std::find_if(queue.begin(), queue.end(),
                            [&](const Buffer& buffer) { return buffer.slot == slot; });
    ASSERT(itr != queue.end());
    ASSERT(itr->status == Buffer::Status::Dequeued);
    itr->status = Buffer::Status::Queued;
    itr->transform = transform;
    itr->crop_rect = crop_rect;
}

boost::optional<const BufferQueue::Buffer&> BufferQueue::AcquireBuffer() {
    auto itr = std::find_if(queue.begin(), queue.end(), [](const Buffer& buffer) {
        return buffer.status == Buffer::Status::Queued;
    });
    if (itr == queue.end())
        return boost::none;
    itr->status = Buffer::Status::Acquired;
    return *itr;
}

void BufferQueue::ReleaseBuffer(u32 slot) {
    auto itr = std::find_if(queue.begin(), queue.end(),
                            [&](const Buffer& buffer) { return buffer.slot == slot; });
    ASSERT(itr != queue.end());
    ASSERT(itr->status == Buffer::Status::Acquired);
    itr->status = Buffer::Status::Free;

    buffer_wait_event->Signal();
}

u32 BufferQueue::Query(QueryType type) {
    LOG_WARNING(Service, "(STUBBED) called type={}", static_cast<u32>(type));
    switch (type) {
    case QueryType::NativeWindowFormat:
        // TODO(Subv): Use an enum for this
        static constexpr u32 FormatABGR8 = 1;
        return FormatABGR8;
    }

    UNIMPLEMENTED();
    return 0;
}

} // namespace Service::NVFlinger
