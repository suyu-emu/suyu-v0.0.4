// Copyright 2021 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/kernel/k_event.h"
#include "core/hle/kernel/k_readable_event.h"
#include "core/hle/kernel/k_writable_event.h"

namespace Kernel {

KEvent::KEvent(KernelCore& kernel, std::string&& name) : Object{kernel, std::move(name)} {}

KEvent::~KEvent() = default;

std::shared_ptr<KEvent> KEvent::Create(KernelCore& kernel, std::string&& name) {
    return std::make_shared<KEvent>(kernel, std::move(name));
}

void KEvent::Initialize() {
    // Create our sub events.
    readable_event = std::make_shared<KReadableEvent>(kernel, GetName() + ":Readable");
    writable_event = std::make_shared<KWritableEvent>(kernel, GetName() + ":Writable");

    // Initialize our sub sessions.
    readable_event->Initialize(this);
    writable_event->Initialize(this);

    // Mark initialized.
    initialized = true;
}

} // namespace Kernel
