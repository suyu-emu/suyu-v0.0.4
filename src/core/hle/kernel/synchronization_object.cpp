// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include "common/assert.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/synchronization.h"
#include "core/hle/kernel/synchronization_object.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {

SynchronizationObject::SynchronizationObject(KernelCore& kernel) : Object{kernel} {}
SynchronizationObject::~SynchronizationObject() = default;

void SynchronizationObject::Signal() {
    kernel.Synchronization().SignalObject(*this);
}

void SynchronizationObject::AddWaitingThread(std::shared_ptr<Thread> thread) {
    auto itr = std::find(waiting_threads.begin(), waiting_threads.end(), thread);
    if (itr == waiting_threads.end())
        waiting_threads.push_back(std::move(thread));
}

void SynchronizationObject::RemoveWaitingThread(std::shared_ptr<Thread> thread) {
    auto itr = std::find(waiting_threads.begin(), waiting_threads.end(), thread);
    // If a thread passed multiple handles to the same object,
    // the kernel might attempt to remove the thread from the object's
    // waiting threads list multiple times.
    if (itr != waiting_threads.end())
        waiting_threads.erase(itr);
}

std::shared_ptr<Thread> SynchronizationObject::GetHighestPriorityReadyThread() const {
    Thread* candidate = nullptr;
    u32 candidate_priority = THREADPRIO_LOWEST + 1;

    for (const auto& thread : waiting_threads) {
        const ThreadStatus thread_status = thread->GetStatus();

        // The list of waiting threads must not contain threads that are not waiting to be awakened.
        ASSERT_MSG(thread_status == ThreadStatus::WaitSynch ||
                       thread_status == ThreadStatus::WaitHLEEvent,
                   "Inconsistent thread statuses in waiting_threads");

        if (thread->GetPriority() >= candidate_priority)
            continue;

        if (ShouldWait(thread.get()))
            continue;

        candidate = thread.get();
        candidate_priority = thread->GetPriority();
    }

    return SharedFrom(candidate);
}

void SynchronizationObject::WakeupWaitingThread(std::shared_ptr<Thread> thread) {
    ASSERT(!ShouldWait(thread.get()));

    if (!thread) {
        return;
    }

    if (thread->IsSleepingOnWait()) {
        for (const auto& object : thread->GetSynchronizationObjects()) {
            ASSERT(!object->ShouldWait(thread.get()));
            object->Acquire(thread.get());
        }
    } else {
        Acquire(thread.get());
    }

    const std::size_t index = thread->GetSynchronizationObjectIndex(SharedFrom(this));

    thread->ClearSynchronizationObjects();

    thread->CancelWakeupTimer();

    bool resume = true;
    if (thread->HasWakeupCallback()) {
        resume = thread->InvokeWakeupCallback(ThreadWakeupReason::Signal, thread, SharedFrom(this),
                                              index);
    }
    if (resume) {
        thread->ResumeFromWait();
        kernel.PrepareReschedule(thread->GetProcessorID());
    }
}

void SynchronizationObject::WakeupAllWaitingThreads() {
    while (auto thread = GetHighestPriorityReadyThread()) {
        WakeupWaitingThread(thread);
    }
}

const std::vector<std::shared_ptr<Thread>>& SynchronizationObject::GetWaitingThreads() const {
    return waiting_threads;
}

} // namespace Kernel
