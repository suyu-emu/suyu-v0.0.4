// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include "common/assert.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/readable_event.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/writable_event.h"

namespace Kernel {

ReadableEvent::ReadableEvent(KernelCore& kernel) : WaitObject{kernel} {}
ReadableEvent::~ReadableEvent() = default;

bool ReadableEvent::ShouldWait(Thread* thread) const {
    return !writable_event->IsSignaled();
}

void ReadableEvent::Acquire(Thread* thread) {
    ASSERT_MSG(!ShouldWait(thread), "object unavailable!");

    writable_event->ResetOnAcquire();
}

void ReadableEvent::AddWaitingThread(SharedPtr<Thread> thread) {
    writable_event->AddWaitingThread(thread);
}

void ReadableEvent::RemoveWaitingThread(Thread* thread) {
    writable_event->RemoveWaitingThread(thread);
}

void ReadableEvent::Signal() {
    writable_event->Signal();
}

void ReadableEvent::Clear() {
    writable_event->Clear();
}

void ReadableEvent::WakeupAllWaitingThreads() {
    writable_event->WakeupAllWaitingThreads();
    writable_event->ResetOnWakeup();
}

} // namespace Kernel
