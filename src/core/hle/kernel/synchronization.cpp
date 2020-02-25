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
#include "core/hle/kernel/time_manager.h"

namespace Kernel {

Synchronization::Synchronization(Core::System& system) : system{system} {}

void Synchronization::SignalObject(SynchronizationObject& obj) const {
    SchedulerLock lock(system.Kernel());
    if (obj.IsSignaled()) {
        for (auto thread : obj.GetWaitingThreads()) {
            if (thread->GetSchedulingStatus() == ThreadSchedStatus::Paused) {
                thread->SetSynchronizationResults(&obj, RESULT_SUCCESS);
                thread->ResumeFromWait();
            }
        }
    }
}

std::pair<ResultCode, Handle> Synchronization::WaitFor(
    std::vector<std::shared_ptr<SynchronizationObject>>& sync_objects, s64 nano_seconds) {
    auto& kernel = system.Kernel();
    auto* const thread = system.CurrentScheduler().GetCurrentThread();
    Handle event_handle = InvalidHandle;
    {
        SchedulerLockAndSleep lock(kernel, event_handle, thread, nano_seconds);
        const auto itr =
            std::find_if(sync_objects.begin(), sync_objects.end(),
                         [thread](const std::shared_ptr<SynchronizationObject>& object) {
                             return object->IsSignaled();
                         });

        if (itr != sync_objects.end()) {
            // We found a ready object, acquire it and set the result value
            SynchronizationObject* object = itr->get();
            object->Acquire(thread);
            const u32 index = static_cast<s32>(std::distance(sync_objects.begin(), itr));
            lock.CancelSleep();
            return {RESULT_SUCCESS, index};
        }

        if (nano_seconds == 0) {
            lock.CancelSleep();
            return {RESULT_TIMEOUT, InvalidHandle};
        }

        /// TODO(Blinkhawk): Check for termination pending

        if (thread->IsSyncCancelled()) {
            thread->SetSyncCancelled(false);
            lock.CancelSleep();
            return {ERR_SYNCHRONIZATION_CANCELED, InvalidHandle};
        }

        for (auto& object : sync_objects) {
            object->AddWaitingThread(SharedFrom(thread));
        }
        thread->SetSynchronizationResults(nullptr, RESULT_TIMEOUT);
        thread->SetStatus(ThreadStatus::WaitSynch);
    }

    if (event_handle != InvalidHandle) {
        auto& time_manager = kernel.TimeManager();
        time_manager.UnscheduleTimeEvent(event_handle);
    }

    {
        SchedulerLock lock(kernel);
        ResultCode signaling_result = thread->GetSignalingResult();
        SynchronizationObject* signaling_object = thread->GetSignalingObject();
        if (signaling_result == RESULT_SUCCESS) {
            const auto itr = std::find_if(
                sync_objects.begin(), sync_objects.end(),
                [signaling_object](const std::shared_ptr<SynchronizationObject>& object) {
                    return object.get() == signaling_object;
                });
            ASSERT(itr != sync_objects.end());
            signaling_object->Acquire(thread);
            const u32 index = static_cast<s32>(std::distance(sync_objects.begin(), itr));
            return {RESULT_SUCCESS, index};
        }
        return {signaling_result, -1};
    }
}

} // namespace Kernel
