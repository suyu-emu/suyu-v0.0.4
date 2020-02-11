// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>

#include "core/hle/kernel/object.h"

namespace Kernel {

class KernelCore;
class Thread;

/// Class that represents a Kernel object that a thread can be waiting on
class SynchronizationObject : public Object {
public:
    explicit SynchronizationObject(KernelCore& kernel);
    ~SynchronizationObject() override;

    /**
     * Check if the specified thread should wait until the object is available
     * @param thread The thread about which we're deciding.
     * @return True if the current thread should wait due to this object being unavailable
     */
    virtual bool ShouldWait(const Thread* thread) const = 0;

    /// Acquire/lock the object for the specified thread if it is available
    virtual void Acquire(Thread* thread) = 0;

    /// Signal this object
    virtual void Signal();

    virtual bool IsSignaled() const {
        return is_signaled;
    }

    /**
     * Add a thread to wait on this object
     * @param thread Pointer to thread to add
     */
    void AddWaitingThread(std::shared_ptr<Thread> thread);

    /**
     * Removes a thread from waiting on this object (e.g. if it was resumed already)
     * @param thread Pointer to thread to remove
     */
    void RemoveWaitingThread(std::shared_ptr<Thread> thread);

    /**
     * Wake up all threads waiting on this object that can be awoken, in priority order,
     * and set the synchronization result and output of the thread.
     */
    void WakeupAllWaitingThreads();

    /**
     * Wakes up a single thread waiting on this object.
     * @param thread Thread that is waiting on this object to wakeup.
     */
    void WakeupWaitingThread(std::shared_ptr<Thread> thread);

    /// Obtains the highest priority thread that is ready to run from this object's waiting list.
    std::shared_ptr<Thread> GetHighestPriorityReadyThread() const;

    /// Get a const reference to the waiting threads list for debug use
    const std::vector<std::shared_ptr<Thread>>& GetWaitingThreads() const;

protected:
    bool is_signaled{}; // Tells if this sync object is signalled;

private:
    /// Threads waiting for this object to become available
    std::vector<std::shared_ptr<Thread>> waiting_threads;
};

// Specialization of DynamicObjectCast for SynchronizationObjects
template <>
inline std::shared_ptr<SynchronizationObject> DynamicObjectCast<SynchronizationObject>(
    std::shared_ptr<Object> object) {
    if (object != nullptr && object->IsWaitable()) {
        return std::static_pointer_cast<SynchronizationObject>(object);
    }
    return nullptr;
}

} // namespace Kernel
