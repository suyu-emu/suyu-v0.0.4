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

void SynchronizationObject::ClearWaitingThreads() {
    waiting_threads.clear();
}

const std::vector<std::shared_ptr<Thread>>& SynchronizationObject::GetWaitingThreads() const {
    return waiting_threads;
}

} // namespace Kernel
