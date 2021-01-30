// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include "common/assert.h"
#include "core/hle/kernel/k_readable_event.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/k_writable_event.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/object.h"

namespace Kernel {

KWritableEvent::KWritableEvent(KernelCore& kernel) : Object{kernel} {}
KWritableEvent::~KWritableEvent() = default;

EventPair KWritableEvent::CreateEventPair(KernelCore& kernel, std::string name) {
    std::shared_ptr<KWritableEvent> writable_event(new KWritableEvent(kernel));
    std::shared_ptr<KReadableEvent> readable_event(new KReadableEvent(kernel));

    writable_event->name = name + ":Writable";
    writable_event->readable = readable_event;
    readable_event->name = name + ":Readable";

    return {std::move(readable_event), std::move(writable_event)};
}

std::shared_ptr<KReadableEvent> KWritableEvent::GetReadableEvent() const {
    return readable;
}

void KWritableEvent::Signal() {
    readable->Signal();
}

void KWritableEvent::Clear() {
    readable->Clear();
}

} // namespace Kernel
