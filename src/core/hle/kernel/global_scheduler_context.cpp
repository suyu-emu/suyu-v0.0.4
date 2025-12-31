// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <mutex>
#include <ranges>

#include "common/assert.h"
#include "core/core.h"
#include "core/hle/kernel/global_scheduler_context.h"
#include "core/hle/kernel/k_scheduler.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/physical_core.h"

namespace Kernel {

GlobalSchedulerContext::GlobalSchedulerContext(KernelCore& kernel)
    : m_kernel{kernel}, m_scheduler_lock{kernel} {}

GlobalSchedulerContext::~GlobalSchedulerContext() = default;

/// @brief Adds a new thread to the scheduler
void GlobalSchedulerContext::AddThread(KThread* thread) noexcept {
    std::scoped_lock lock{m_global_list_guard};
    m_thread_list.push_back(thread);
}

/// @brief Removes a thread from the scheduler
void GlobalSchedulerContext::RemoveThread(KThread* thread) noexcept {
    std::scoped_lock lock{m_global_list_guard};
    m_thread_list.erase(std::ranges::find(m_thread_list, thread));
}

/// @brief Rotates the scheduling queues of threads at a preemption priority
///  and then does some core rebalancing. Preemption priorities can be found
/// in the array 'preemption_priorities'.
/// @note This operation happens every 10ms.
void GlobalSchedulerContext::PreemptThreads() noexcept {
    // The priority levels at which the global scheduler preempts threads every 10 ms. They are
    // ordered from Core 0 to Core 3.
    static constexpr std::array<u32, Core::Hardware::NUM_CPU_CORES> per_core{
        59,
        59,
        59,
        63,
    };
    ASSERT(KScheduler::IsSchedulerLockedByCurrentThread(m_kernel));
    for (u32 core_id = 0; core_id < per_core.size(); core_id++)
        KScheduler::RotateScheduledQueue(m_kernel, core_id, per_core[core_id]);
}

/// @brief Returns true if the global scheduler lock is acquired
bool GlobalSchedulerContext::IsLocked() const noexcept {
    return m_scheduler_lock.IsLockedByCurrentThread();
}

void GlobalSchedulerContext::RegisterDummyThreadForWakeup(KThread* thread) noexcept {
    ASSERT(this->IsLocked());
    m_woken_dummy_threads.push_back(thread);
}

void GlobalSchedulerContext::UnregisterDummyThreadForWakeup(KThread* thread) noexcept {
    ASSERT(this->IsLocked());
    if(auto it = std::ranges::find(m_woken_dummy_threads, thread); it != m_woken_dummy_threads.end()) {
        *it = m_woken_dummy_threads.back();
        m_woken_dummy_threads.pop_back();
    }
}

void GlobalSchedulerContext::WakeupWaitingDummyThreads() noexcept {
    ASSERT(this->IsLocked());
    if (m_woken_dummy_threads.size() > 0) {
        for (auto* thread : m_woken_dummy_threads)
            thread->DummyThreadEndWait();
        m_woken_dummy_threads.clear();
    }
}

} // namespace Kernel
