// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/kernel/physical_core.h"
#include "core/hle/kernel/svc.h"

namespace Kernel::Svc {

/// Get which CPU core is executing the current thread
u32 GetCurrentProcessorNumber(Core::System& system) {
    LOG_TRACE(Kernel_SVC, "called");
    return static_cast<u32>(system.CurrentPhysicalCore().CoreIndex());
}

u32 GetCurrentProcessorNumber32(Core::System& system) {
    return GetCurrentProcessorNumber(system);
}

} // namespace Kernel::Svc
