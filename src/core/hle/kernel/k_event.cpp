// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/kernel/k_event.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_resource_limit.h"

namespace Kernel {

KEvent::KEvent(KernelCore& kernel)
    : KAutoObjectWithSlabHeapAndContainer{kernel}
    , m_readable_event{kernel}
{}

KEvent::~KEvent() = default;

void KEvent::Initialize(KernelCore& kernel, KProcess* owner) {
    // Create our readable event.
    KAutoObject::Create(std::addressof(m_readable_event));

    // Initialize our readable event.
    m_readable_event.Initialize(kernel, this);

    // Set our owner process.
    // HACK: this should never be nullptr, but service threads don't have a
    // proper parent process yet.
    if (owner != nullptr) {
        m_owner = owner;
        m_owner->Open(kernel);
    }

    // Mark initialized.
    m_initialized = true;
}

void KEvent::Finalize(KernelCore& kernel) {
    KAutoObjectWithSlabHeapAndContainer<KEvent, KAutoObjectWithList>::Finalize(kernel);
}

Result KEvent::Signal(KernelCore& kernel) {
    KScopedSchedulerLock sl{kernel};

    R_SUCCEED_IF(m_readable_event_destroyed);

    return m_readable_event.Signal(kernel);
}

Result KEvent::Clear(KernelCore& kernel) {
    KScopedSchedulerLock sl{kernel};

    R_SUCCEED_IF(m_readable_event_destroyed);

    return m_readable_event.Clear(kernel);
}

void KEvent::PostDestroy(KernelCore& kernel, uintptr_t arg) {
    // Release the event count resource the owner process holds.
    KProcess* owner = reinterpret_cast<KProcess*>(arg);

    if (owner != nullptr) {
        owner->GetResourceLimit()->Release(kernel, LimitableResource::EventCountMax, 1);
        owner->Close(kernel);
    }
}

} // namespace Kernel
