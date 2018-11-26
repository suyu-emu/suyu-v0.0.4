// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include "common/assert.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/readable_event.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/writable_event.h"

namespace Kernel {

WritableEvent::WritableEvent(KernelCore& kernel) : WaitObject{kernel} {}
WritableEvent::~WritableEvent() = default;

std::tuple<SharedPtr<WritableEvent>, SharedPtr<ReadableEvent>> WritableEvent::CreateEventPair(
    KernelCore& kernel, ResetType reset_type, std::string name) {
    SharedPtr<WritableEvent> writable_event(new WritableEvent(kernel));
    SharedPtr<ReadableEvent> readable_event(new ReadableEvent(kernel));

    writable_event->name = name + ":Writable";
    writable_event->signaled = false;
    writable_event->reset_type = reset_type;
    readable_event->name = name + ":Readable";
    readable_event->writable_event = writable_event;

    return std::make_tuple(std::move(writable_event), std::move(readable_event));
}

SharedPtr<WritableEvent> WritableEvent::CreateRegisteredEventPair(KernelCore& kernel,
                                                                  ResetType reset_type,
                                                                  std::string name) {
    auto [writable_event, readable_event] = CreateEventPair(kernel, reset_type, name);
    kernel.AddNamedEvent(name, std::move(readable_event));
    return std::move(writable_event);
}

bool WritableEvent::ShouldWait(Thread* thread) const {
    return !signaled;
}

void WritableEvent::Acquire(Thread* thread) {
    ASSERT_MSG(!ShouldWait(thread), "object unavailable!");

    if (reset_type == ResetType::OneShot)
        signaled = false;
}

void WritableEvent::Signal() {
    signaled = true;
    WakeupAllWaitingThreads();
}

void WritableEvent::Clear() {
    signaled = false;
}

void WritableEvent::ResetOnAcquire() {
    if (reset_type == ResetType::OneShot)
        Clear();
}

void WritableEvent::ResetOnWakeup() {
    if (reset_type == ResetType::Pulse)
        Clear();
}

bool WritableEvent::IsSignaled() const {
    return signaled;
}

void WritableEvent::WakeupAllWaitingThreads() {
    WaitObject::WakeupAllWaitingThreads();

    if (reset_type == ResetType::Pulse)
        signaled = false;
}

} // namespace Kernel
