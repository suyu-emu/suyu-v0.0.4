// Copyright 2021 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/kernel/k_event.h"
#include "core/hle/kernel/k_readable_event.h"
#include "core/hle/kernel/k_resource_limit.h"
#include "core/hle/kernel/k_writable_event.h"
#include "core/hle/kernel/process.h"

namespace Kernel {

KEvent::KEvent(KernelCore& kernel) : KAutoObjectWithSlabHeapAndContainer{kernel} {}

KEvent::~KEvent() = default;

void KEvent::Initialize(std::string&& name_) {
    // Increment reference count.
    // Because reference count is one on creation, this will result
    // in a reference count of two. Thus, when both readable and
    // writable events are closed this object will be destroyed.
    Open();

    // Create our sub events.
    readable_event = std::make_shared<KReadableEvent>(kernel, name_ + ":Readable");
    writable_event = std::make_shared<KWritableEvent>(kernel, name_ + ":Writable");

    // Initialize our sub sessions.
    readable_event->Initialize(this);
    writable_event->Initialize(this);

    // Set our owner process.
    owner = kernel.CurrentProcess();
    if (owner) {
        owner->Open();
    }

    // Mark initialized.
    name = std::move(name_);
    initialized = true;
}

void KEvent::Finalize() {
    KAutoObjectWithSlabHeapAndContainer<KEvent, KAutoObjectWithList>::Finalize();
}

void KEvent::PostDestroy(uintptr_t arg) {
    // Release the event count resource the owner process holds.
    Process* owner = reinterpret_cast<Process*>(arg);
    if (owner) {
        owner->GetResourceLimit()->Release(LimitableResource::Events, 1);
        owner->Close();
    }
}

} // namespace Kernel
