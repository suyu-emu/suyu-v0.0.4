// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/timer.h"

namespace Kernel {

WaitObject::WaitObject(KernelCore& kernel) : Object{kernel} {}
WaitObject::~WaitObject() = default;

void WaitObject::AddWaitingThread(SharedPtr<Thread> thread) {
    auto itr = std::find(waiting_threads.begin(), waiting_threads.end(), thread);
    if (itr == waiting_threads.end())
        waiting_threads.push_back(std::move(thread));
}

void WaitObject::RemoveWaitingThread(Thread* thread) {
    auto itr = std::find(waiting_threads.begin(), waiting_threads.end(), thread);
    // If a thread passed multiple handles to the same object,
    // the kernel might attempt to remove the thread from the object's
    // waiting threads list multiple times.
    if (itr != waiting_threads.end())
        waiting_threads.erase(itr);
}

SharedPtr<Thread> WaitObject::GetHighestPriorityReadyThread() {
    Thread* candidate = nullptr;
    u32 candidate_priority = THREADPRIO_LOWEST + 1;

    for (const auto& thread : waiting_threads) {
        // The list of waiting threads must not contain threads that are not waiting to be awakened.
        ASSERT_MSG(thread->status == ThreadStatus::WaitSynchAny ||
                       thread->status == ThreadStatus::WaitSynchAll ||
                       thread->status == ThreadStatus::WaitHLEEvent,
                   "Inconsistent thread statuses in waiting_threads");

        if (thread->current_priority >= candidate_priority)
            continue;

        if (ShouldWait(thread.get()))
            continue;

        // A thread is ready to run if it's either in ThreadStatus::WaitSynchAny or
        // in ThreadStatus::WaitSynchAll and the rest of the objects it is waiting on are ready.
        bool ready_to_run = true;
        if (thread->status == ThreadStatus::WaitSynchAll) {
            ready_to_run = std::none_of(thread->wait_objects.begin(), thread->wait_objects.end(),
                                        [&thread](const SharedPtr<WaitObject>& object) {
                                            return object->ShouldWait(thread.get());
                                        });
        }

        if (ready_to_run) {
            candidate = thread.get();
            candidate_priority = thread->current_priority;
        }
    }

    return candidate;
}

void WaitObject::WakeupWaitingThread(SharedPtr<Thread> thread) {
    ASSERT(!ShouldWait(thread.get()));

    if (!thread)
        return;

    if (!thread->IsSleepingOnWaitAll()) {
        Acquire(thread.get());
    } else {
        for (auto& object : thread->wait_objects) {
            ASSERT(!object->ShouldWait(thread.get()));
            object->Acquire(thread.get());
        }
    }

    size_t index = thread->GetWaitObjectIndex(this);

    for (auto& object : thread->wait_objects)
        object->RemoveWaitingThread(thread.get());
    thread->wait_objects.clear();

    thread->CancelWakeupTimer();

    bool resume = true;

    if (thread->wakeup_callback)
        resume = thread->wakeup_callback(ThreadWakeupReason::Signal, thread, this, index);

    if (resume)
        thread->ResumeFromWait();
}

void WaitObject::WakeupAllWaitingThreads() {
    while (auto thread = GetHighestPriorityReadyThread()) {
        WakeupWaitingThread(thread);
    }
}

const std::vector<SharedPtr<Thread>>& WaitObject::GetWaitingThreads() const {
    return waiting_threads;
}

} // namespace Kernel
