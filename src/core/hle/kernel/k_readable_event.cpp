// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/kernel/k_readable_event.h"
#include "core/hle/kernel/k_scheduler.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/svc_results.h"

namespace Kernel {

KReadableEvent::KReadableEvent(KernelCore& kernel) : KSynchronizationObject{kernel} {}

KReadableEvent::~KReadableEvent() = default;

void KReadableEvent::Initialize(KernelCore& kernel, KEvent* parent) {
    m_is_signaled = false;
    m_parent = parent;
    if (m_parent != nullptr) {
        m_parent->Open(kernel);
    }
}

bool KReadableEvent::IsSignaled(KernelCore& kernel) const {
    ASSERT(KScheduler::IsSchedulerLockedByCurrentThread(kernel));
    return m_is_signaled;
}

void KReadableEvent::Destroy(KernelCore& kernel) {
    if (m_parent) {
        {
            KScopedSchedulerLock sl{kernel};
            m_parent->OnReadableEventDestroyed();
        }
        m_parent->Close(kernel);
    }
}

Result KReadableEvent::Signal(KernelCore& kernel) {
    KScopedSchedulerLock lk{kernel};
    if (!m_is_signaled) {
        m_is_signaled = true;
        this->NotifyAvailable(kernel);
    }
    R_SUCCEED();
}

Result KReadableEvent::Clear(KernelCore& kernel) {
    this->Reset(kernel);
    R_SUCCEED();
}

Result KReadableEvent::Reset(KernelCore& kernel) {
    KScopedSchedulerLock lk{kernel};

    R_UNLESS(m_is_signaled, ResultInvalidState);

    m_is_signaled = false;
    R_SUCCEED();
}

} // namespace Kernel
