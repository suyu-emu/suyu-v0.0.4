// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/core.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/scheduler.h"
#include "core/hle/kernel/synchronization.h"
#include "core/hle/kernel/synchronization_object.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {

/// Default thread wakeup callback for WaitSynchronization
static bool DefaultThreadWakeupCallback(ThreadWakeupReason reason, std::shared_ptr<Thread> thread,
                                        std::shared_ptr<SynchronizationObject> object,
                                        std::size_t index) {
    ASSERT(thread->GetStatus() == ThreadStatus::WaitSynch);

    if (reason == ThreadWakeupReason::Timeout) {
        thread->SetWaitSynchronizationResult(RESULT_TIMEOUT);
        return true;
    }

    ASSERT(reason == ThreadWakeupReason::Signal);
    thread->SetWaitSynchronizationResult(RESULT_SUCCESS);
    thread->SetWaitSynchronizationOutput(static_cast<u32>(index));
    return true;
}

Synchronization::Synchronization(Core::System& system) : system{system} {}

void Synchronization::SignalObject(SynchronizationObject& obj) const {
    if (obj.IsSignaled()) {
        obj.WakeupAllWaitingThreads();
    }
}

std::pair<ResultCode, Handle> Synchronization::WaitFor(
    std::vector<std::shared_ptr<SynchronizationObject>>& sync_objects, s64 nano_seconds) {
    auto* const thread = system.CurrentScheduler().GetCurrentThread();
    // Find the first object that is acquirable in the provided list of objects
    const auto itr = std::find_if(sync_objects.begin(), sync_objects.end(),
                                  [thread](const std::shared_ptr<SynchronizationObject>& object) {
                                      return object->IsSignaled();
                                  });

    if (itr != sync_objects.end()) {
        // We found a ready object, acquire it and set the result value
        SynchronizationObject* object = itr->get();
        object->Acquire(thread);
        const u32 index = static_cast<s32>(std::distance(sync_objects.begin(), itr));
        return {RESULT_SUCCESS, index};
    }

    // No objects were ready to be acquired, prepare to suspend the thread.

    // If a timeout value of 0 was provided, just return the Timeout error code instead of
    // suspending the thread.
    if (nano_seconds == 0) {
        return {RESULT_TIMEOUT, InvalidHandle};
    }

    if (thread->IsSyncCancelled()) {
        thread->SetSyncCancelled(false);
        return {ERR_SYNCHRONIZATION_CANCELED, InvalidHandle};
    }

    for (auto& object : sync_objects) {
        object->AddWaitingThread(SharedFrom(thread));
    }

    thread->SetSynchronizationObjects(std::move(sync_objects));
    thread->SetStatus(ThreadStatus::WaitSynch);

    // Create an event to wake the thread up after the specified nanosecond delay has passed
    thread->WakeAfterDelay(nano_seconds);
    thread->SetWakeupCallback(DefaultThreadWakeupCallback);

    system.PrepareReschedule(thread->GetProcessorID());

    return {RESULT_TIMEOUT, InvalidHandle};
}

} // namespace Kernel
