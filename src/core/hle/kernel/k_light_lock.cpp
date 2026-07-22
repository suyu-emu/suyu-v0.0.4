// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/kernel/k_light_lock.h"
#include "core/hle/kernel/k_scheduler.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/k_thread_queue.h"
#include "core/hle/kernel/kernel.h"

namespace Kernel {

namespace {

class ThreadQueueImplForKLightLock final : public KThreadQueue {
public:
    explicit ThreadQueueImplForKLightLock(KernelCore& kernel) : KThreadQueue(kernel) {}

    void CancelWait(KernelCore& kernel, KThread* waiting_thread, Result wait_result, bool cancel_timer_task) override {
        // Remove the thread as a waiter from its owner.
        if (KThread* owner = waiting_thread->GetLockOwner(kernel); owner != nullptr) {
            owner->RemoveWaiter(kernel, waiting_thread);
        }

        // Invoke the base cancel wait handler.
        KThreadQueue::CancelWait(kernel, waiting_thread, wait_result, cancel_timer_task);
    }
};

} // namespace

void KLightLock::Lock() {
    const uintptr_t cur_thread = uintptr_t(GetCurrentThreadPointer(m_kernel));
    while (true) {
        uintptr_t old_tag = m_tag.load(std::memory_order_relaxed);
        while (!m_tag.compare_exchange_weak(old_tag, (old_tag == 0) ? cur_thread : (old_tag | 1), std::memory_order_acquire))
            ;
        if (old_tag == 0 || this->LockSlowPath(old_tag | 1, cur_thread)) {
            break;
        }
    }
}

void KLightLock::Unlock() {
    const uintptr_t cur_thread = reinterpret_cast<uintptr_t>(GetCurrentThreadPointer(m_kernel));

    uintptr_t expected = cur_thread;
    if (!m_tag.compare_exchange_strong(expected, 0, std::memory_order_release)) {
        this->UnlockSlowPath(cur_thread);
    }
}

bool KLightLock::LockSlowPath(uintptr_t _owner, uintptr_t _cur_thread) {
    KThread* cur_thread = reinterpret_cast<KThread*>(_cur_thread);
    ThreadQueueImplForKLightLock wait_queue(m_kernel);

    // Pend the current thread waiting on the owner thread.
    {
        KScopedSchedulerLock sl{m_kernel};

        // Ensure we actually have locking to do.
        if (m_tag.load(std::memory_order_relaxed) != _owner) {
            return false;
        }

        // Add the current thread as a waiter on the owner.
        KThread* owner_thread = reinterpret_cast<KThread*>(_owner & ~1ULL);
        cur_thread->SetKernelAddressKey(reinterpret_cast<uintptr_t>(std::addressof(m_tag)));
        owner_thread->AddWaiter(m_kernel, cur_thread);

        // Begin waiting to hold the lock.
        cur_thread->BeginWait(m_kernel, std::addressof(wait_queue));

        if (owner_thread->IsSuspended()) {
            owner_thread->ContinueIfHasKernelWaiters(m_kernel);
        }
    }

    return true;
}

void KLightLock::UnlockSlowPath(uintptr_t _cur_thread) {
    KThread* owner_thread = reinterpret_cast<KThread*>(_cur_thread);

    // Unlock.
    {
        KScopedSchedulerLock sl(m_kernel);

        // Get the next owner.
        bool has_waiters;
        KThread* next_owner = owner_thread->RemoveKernelWaiterByKey(m_kernel,
            std::addressof(has_waiters), uintptr_t(std::addressof(m_tag)));

        // Pass the lock to the next owner.
        uintptr_t next_tag = 0;
        if (next_owner != nullptr) {
            next_tag = uintptr_t(next_owner) | uintptr_t(has_waiters);

            next_owner->EndWait(m_kernel, ResultSuccess);

            if (next_owner->IsSuspended()) {
                next_owner->ContinueIfHasKernelWaiters(m_kernel);
            }
        }

        // We may have unsuspended in the process of acquiring the lock, so we'll re-suspend now if
        // so.
        if (owner_thread->IsSuspended()) {
            owner_thread->TrySuspend(m_kernel);
        }

        // Write the new tag value.
        m_tag.store(next_tag, std::memory_order_release);
    }
}

bool KLightLock::IsLockedByCurrentThread() const {
    return (m_tag.load() | 1ULL) == (uintptr_t(GetCurrentThreadPointer(m_kernel)) | 1ULL);
}

} // namespace Kernel
