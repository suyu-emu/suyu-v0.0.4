// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging/log.h"
#include "core/arm/arm_interface.h"
#include "core/arm/debug.h"
#include "core/core.h"
#include "core/hle/kernel/k_process.h"

namespace Core {

void ArmInterface::LogBacktrace(Kernel::KProcess* process) const {
    Kernel::Svc::ThreadContext ctx;
    this->GetContext(ctx);

    LOG_ERROR(Core_ARM, "Backtrace, sp={:016X}, pc={:016X}", ctx.sp, ctx.pc);
    LOG_ERROR(Core_ARM, "{:20}{:20}{:20}{:20}{}", "Module Name", "Address", "Original Address",
              "Offset", "Symbol");
    LOG_ERROR(Core_ARM, "");
    const auto backtrace = GetBacktraceFromContext(process, ctx);
    for (const auto& entry : backtrace) {
        LOG_ERROR(Core_ARM, "{:20}{:016X}    {:016X}    {:016X}    {}", entry.module, entry.address,
                  entry.original_address, entry.offset, entry.name);
    }
}

const Kernel::DebugWatchpoint* ArmInterface::MatchingWatchpoint(
    u64 addr, u64 size, Kernel::DebugWatchpointType access_type) const {
    if (!m_watchpoints) {
        return nullptr;
    }

    const u64 start_address{addr};
    const u64 end_address{addr + size};

    for (size_t i = 0; i < Core::Hardware::NUM_WATCHPOINTS; i++) {
        const auto& watch{(*m_watchpoints)[i]};

        if (end_address <= GetInteger(watch.start_address)) {
            continue;
        }
        if (start_address >= GetInteger(watch.end_address)) {
            continue;
        }
        if ((access_type & watch.type) == Kernel::DebugWatchpointType::None) {
            continue;
        }

        return &watch;
    }

    return nullptr;
}

} // namespace Core
