// Copyright 2021 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/kernel/k_event.h"
#include "core/hle/kernel/k_readable_event.h"
#include "core/hle/kernel/k_writable_event.h"

namespace Kernel {

KWritableEvent::KWritableEvent(KernelCore& kernel, std::string&& name)
    : Object{kernel, std::move(name)} {}
KWritableEvent::~KWritableEvent() = default;

void KWritableEvent::Initialize(KEvent* parent_) {
    parent = parent_;
}

ResultCode KWritableEvent::Signal() {
    return parent->GetReadableEvent()->Signal();
}

ResultCode KWritableEvent::Clear() {
    return parent->GetReadableEvent()->Clear();
}

} // namespace Kernel
