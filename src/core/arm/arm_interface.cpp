// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
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

    std::array<u64, 32> xreg{
        ctx.r[0], ctx.r[1], ctx.r[2], ctx.r[3],
        ctx.r[4], ctx.r[5], ctx.r[6], ctx.r[7],
        ctx.r[8], ctx.r[9], ctx.r[10], ctx.r[11],
        ctx.r[12], ctx.r[13], ctx.r[14], ctx.r[15],
        ctx.r[16], ctx.r[17], ctx.r[18], ctx.r[19],
        ctx.r[20], ctx.r[21], ctx.r[22], ctx.r[23],
        ctx.r[24], ctx.r[25], ctx.r[26], ctx.r[27],
        ctx.r[28], ctx.fp, ctx.lr, ctx.sp,
    };

    std::string msg = fmt::format("Backtrace @ PC={:016X}\n", ctx.pc);
    for (size_t i = 0; i < 32; i += 4)
        msg += fmt::format("R{:02}={:016X} R{:02}={:016X} R{:02}={:016X} R{:02}={:016X}\n",
            i + 0, xreg[i + 0], i + 1, xreg[i + 1],
            i + 2, xreg[i + 2], i + 3, xreg[i + 3]);
    for (size_t i = 0; i < 32; i += 2)
        msg += fmt::format("V{:02}={:016X}_{:016X} V{:02}={:016X}_{:016X}\n",
            i + 0, ctx.v[i + 0][0], ctx.v[i + 0][1],
            i + 1, ctx.v[i + 1][0], ctx.v[i + 1][1]);
    msg += fmt::format("PSTATE={:08X} FPCR={:08X} FPSR={:08X} TPIDR={:016X}\n", ctx.pstate, ctx.fpcr, ctx.fpsr, ctx.tpidr);
    msg += fmt::format("{:20}{:20}{:20}{:20}{}\n", "Module", "Address", "Original Address", "Offset", "Symbol");
    auto const backtrace = GetBacktraceFromContext(process, ctx);
    for (auto const& entry : backtrace)
        msg += fmt::format("{:20}{:016X}    {:016X}    {:016X}    {}\n", entry.module, entry.address, entry.original_address, entry.offset, entry.name);
    LOG_ERROR(Core_ARM, "{}", msg);
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
