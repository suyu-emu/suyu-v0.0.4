// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/core.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/k_scheduler.h"
#include "core/hle/kernel/k_scoped_scheduler_lock_and_sleep.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/synchronization.h"
#include "core/hle/kernel/synchronization_object.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/time_manager.h"

namespace Kernel {

Synchronization::Synchronization(Core::System& system) : system{system} {}

void Synchronization::SignalObject(SynchronizationObject& obj) const {
    auto& kernel = system.Kernel();
    KScopedSchedulerLock lock(kernel);
    if (obj.IsSignaled()) {
        for (auto thread : obj.GetWaitingThreads()) {
            if (thread->GetSchedulingStatus() == ThreadSchedStatus::Paused) {
                if (thread->GetStatus() != ThreadStatus::WaitHLEEvent) {
                    ASSERT(thread->GetStatus() == ThreadStatus::WaitSynch);
                    ASSERT(thread->IsWaitingSync());
                }
                thread->SetSynchronizationResults(&obj, RESULT_SUCCESS);
                thread->ResumeFromWait();
            }
        }
        obj.ClearWaitingThreads();
    }
}

std::pair<ResultCode, Handle> Synchronization::WaitFor(
    std::vector<std::shared_ptr<SynchronizationObject>>& sync_objects, s64 nano_seconds) {
    auto& kernel = system.Kernel();
    auto* const thread = kernel.CurrentScheduler()->GetCurrentThread();
    Handle event_handle = InvalidHandle;
    {
        KScopedSchedulerLockAndSleep lock(kernel, event_handle, thread, nano_seconds);
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

        if (thread->IsPendingTermination()) {
            lock.CancelSleep();
            return {ERR_THREAD_TERMINATING, InvalidHandle};
        }

        if (thread->IsSyncCancelled()) {
            thread->SetSyncCancelled(false);
            lock.CancelSleep();
            return {ERR_SYNCHRONIZATION_CANCELED, InvalidHandle};
        }

        for (auto& object : sync_objects) {
            object->AddWaitingThread(SharedFrom(thread));
        }

        thread->SetSynchronizationObjects(&sync_objects);
        thread->SetSynchronizationResults(nullptr, RESULT_TIMEOUT);
        thread->SetStatus(ThreadStatus::WaitSynch);
        thread->SetWaitingSync(true);
    }
    thread->SetWaitingSync(false);

    if (event_handle != InvalidHandle) {
        auto& time_manager = kernel.TimeManager();
        time_manager.UnscheduleTimeEvent(event_handle);
    }

    {
        KScopedSchedulerLock lock(kernel);
        ResultCode signaling_result = thread->GetSignalingResult();
        SynchronizationObject* signaling_object = thread->GetSignalingObject();
        thread->SetSynchronizationObjects(nullptr);
        auto shared_thread = SharedFrom(thread);
        for (auto& obj : sync_objects) {
            obj->RemoveWaitingThread(shared_thread);
        }
        if (signaling_object != nullptr) {
            const auto itr = std::find_if(
                sync_objects.begin(), sync_objects.end(),
                [signaling_object](const std::shared_ptr<SynchronizationObject>& object) {
                    return object.get() == signaling_object;
                });
            ASSERT(itr != sync_objects.end());
            signaling_object->Acquire(thread);
            const u32 index = static_cast<s32>(std::distance(sync_objects.begin(), itr));
            return {signaling_result, index};
        }
        return {signaling_result, -1};
    }
}

} // namespace Kernel
