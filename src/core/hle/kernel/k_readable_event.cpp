// Copyright 2021 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include "common/assert.h"
#include "common/common_funcs.h"
#include "common/logging/log.h"
#include "core/hle/kernel/k_readable_event.h"
#include "core/hle/kernel/k_scheduler.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/svc_results.h"

namespace Kernel {

KReadableEvent::KReadableEvent(KernelCore& kernel, std::string&& name)
    : KSynchronizationObject{kernel, std::move(name)} {}
KReadableEvent::~KReadableEvent() = default;

bool KReadableEvent::IsSignaled() const {
    ASSERT(kernel.GlobalSchedulerContext().IsLocked());

    return is_signaled;
}

ResultCode KReadableEvent::Signal() {
    KScopedSchedulerLock lk{kernel};

    if (!is_signaled) {
        is_signaled = true;
        NotifyAvailable();
    }

    return RESULT_SUCCESS;
}

ResultCode KReadableEvent::Clear() {
    Reset();

    return RESULT_SUCCESS;
}

ResultCode KReadableEvent::Reset() {
    KScopedSchedulerLock lk{kernel};

    if (!is_signaled) {
        return ResultInvalidState;
    }

    is_signaled = false;
    return RESULT_SUCCESS;
}

} // namespace Kernel
