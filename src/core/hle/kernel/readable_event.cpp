// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/k_scheduler.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/readable_event.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {

ReadableEvent::ReadableEvent(KernelCore& kernel) : KSynchronizationObject{kernel} {}
ReadableEvent::~ReadableEvent() = default;

void ReadableEvent::Signal() {
    if (is_signaled) {
        return;
    }

    is_signaled = true;
    NotifyAvailable();
}

bool ReadableEvent::IsSignaled() const {
    ASSERT(kernel.GlobalSchedulerContext().IsLocked());

    return is_signaled;
}

void ReadableEvent::Clear() {
    is_signaled = false;
}

ResultCode ReadableEvent::Reset() {
    KScopedSchedulerLock lock(kernel);
    if (!is_signaled) {
        LOG_TRACE(Kernel, "Handle is not signaled! object_id={}, object_type={}, object_name={}",
                  GetObjectId(), GetTypeName(), GetName());
        return ERR_INVALID_STATE;
    }

    Clear();

    return RESULT_SUCCESS;
}

} // namespace Kernel
