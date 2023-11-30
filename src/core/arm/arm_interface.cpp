// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <map>
#include <optional>

#include "common/bit_field.h"
#include "common/common_types.h"
#include "common/demangle.h"
#include "common/logging/log.h"
#include "core/arm/arm_interface.h"
#include "core/arm/symbols.h"
#include "core/core.h"
#include "core/debugger/debugger.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/svc.h"
#include "core/loader/loader.h"
#include "core/memory.h"

namespace Core {

constexpr u64 SEGMENT_BASE = 0x7100000000ull;

std::vector<ARM_Interface::BacktraceEntry> ARM_Interface::GetBacktraceFromContext(
    Core::System& system, const ARM_Interface::ThreadContext32& ctx) {
    std::vector<BacktraceEntry> out;
    auto& memory = system.ApplicationMemory();

    const auto& reg = ctx.cpu_registers;
    u32 pc = reg[15], lr = reg[14], fp = reg[11];
    out.push_back({"", 0, pc, 0, ""});

    // fp (= r11) points to the last frame record.
    // Frame records are two words long:
    // fp+0 : pointer to previous frame record
    // fp+4 : value of lr for frame
    for (size_t i = 0; i < 256; i++) {
        out.push_back({"", 0, lr, 0, ""});
        if (!fp || (fp % 4 != 0) || !memory.IsValidVirtualAddressRange(fp, 8)) {
            break;
        }
        lr = memory.Read32(fp + 4);
        fp = memory.Read32(fp);
    }

    SymbolicateBacktrace(system, out);

    return out;
}

std::vector<ARM_Interface::BacktraceEntry> ARM_Interface::GetBacktraceFromContext(
    Core::System& system, const ARM_Interface::ThreadContext64& ctx) {
    std::vector<BacktraceEntry> out;
    auto& memory = system.ApplicationMemory();

    const auto& reg = ctx.cpu_registers;
    u64 pc = ctx.pc, lr = reg[30], fp = reg[29];

    out.push_back({"", 0, pc, 0, ""});

    // fp (= x29) points to the previous frame record.
    // Frame records are two words long:
    // fp+0 : pointer to previous frame record
    // fp+8 : value of lr for frame
    for (size_t i = 0; i < 256; i++) {
        out.push_back({"", 0, lr, 0, ""});
        if (!fp || (fp % 4 != 0) || !memory.IsValidVirtualAddressRange(fp, 16)) {
            break;
        }
        lr = memory.Read64(fp + 8);
        fp = memory.Read64(fp);
    }

    SymbolicateBacktrace(system, out);

    return out;
}

void ARM_Interface::SymbolicateBacktrace(Core::System& system, std::vector<BacktraceEntry>& out) {
    std::map<VAddr, std::string> modules;
    auto& loader{system.GetAppLoader()};
    if (loader.ReadNSOModules(modules) != Loader::ResultStatus::Success) {
        return;
    }

    std::map<std::string, Symbols::Symbols> symbols;
    for (const auto& module : modules) {
        symbols.insert_or_assign(module.second,
                                 Symbols::GetSymbols(module.first, system.ApplicationMemory(),
                                                     system.ApplicationProcess()->Is64Bit()));
    }

    for (auto& entry : out) {
        VAddr base = 0;
        for (auto iter = modules.rbegin(); iter != modules.rend(); ++iter) {
            const auto& module{*iter};
            if (entry.original_address >= module.first) {
                entry.module = module.second;
                base = module.first;
                break;
            }
        }

        entry.offset = entry.original_address - base;
        entry.address = SEGMENT_BASE + entry.offset;

        if (entry.module.empty()) {
            entry.module = "unknown";
        }

        const auto symbol_set = symbols.find(entry.module);
        if (symbol_set != symbols.end()) {
            const auto symbol = Symbols::GetSymbolName(symbol_set->second, entry.offset);
            if (symbol) {
                entry.name = Common::DemangleSymbol(*symbol);
            }
        }
    }
}

std::vector<ARM_Interface::BacktraceEntry> ARM_Interface::GetBacktrace() const {
    if (GetArchitecture() == Architecture::Aarch64) {
        ThreadContext64 ctx;
        SaveContext(ctx);
        return GetBacktraceFromContext(system, ctx);
    } else {
        ThreadContext32 ctx;
        SaveContext(ctx);
        return GetBacktraceFromContext(system, ctx);
    }
}

void ARM_Interface::LogBacktrace() const {
    const VAddr sp = GetSP();
    const VAddr pc = GetPC();
    LOG_ERROR(Core_ARM, "Backtrace, sp={:016X}, pc={:016X}", sp, pc);
    LOG_ERROR(Core_ARM, "{:20}{:20}{:20}{:20}{}", "Module Name", "Address", "Original Address",
              "Offset", "Symbol");
    LOG_ERROR(Core_ARM, "");
    const auto backtrace = GetBacktrace();
    for (const auto& entry : backtrace) {
        LOG_ERROR(Core_ARM, "{:20}{:016X}    {:016X}    {:016X}    {}", entry.module, entry.address,
                  entry.original_address, entry.offset, entry.name);
    }
}

void ARM_Interface::Run() {
    using Kernel::StepState;
    using Kernel::SuspendType;

    while (true) {
        Kernel::KThread* current_thread{Kernel::GetCurrentThreadPointer(system.Kernel())};
        HaltReason hr{};

        // If the thread is scheduled for termination, exit the thread.
        if (current_thread->HasDpc()) {
            if (current_thread->IsTerminationRequested()) {
                current_thread->Exit();
                UNREACHABLE();
            }
        }

        // Notify the debugger and go to sleep if a step was performed
        // and this thread has been scheduled again.
        if (current_thread->GetStepState() == StepState::StepPerformed) {
            system.GetDebugger().NotifyThreadStopped(current_thread);
            current_thread->RequestSuspend(SuspendType::Debug);
            break;
        }

        // Otherwise, run the thread.
        system.EnterCPUProfile();
        if (current_thread->GetStepState() == StepState::StepPending) {
            hr = StepJit();

            if (True(hr & HaltReason::StepThread)) {
                current_thread->SetStepState(StepState::StepPerformed);
            }
        } else {
            hr = RunJit();
        }
        system.ExitCPUProfile();

        // Notify the debugger and go to sleep if a breakpoint was hit,
        // or if the thread is unable to continue for any reason.
        if (True(hr & HaltReason::InstructionBreakpoint) || True(hr & HaltReason::PrefetchAbort)) {
            if (!True(hr & HaltReason::PrefetchAbort)) {
                RewindBreakpointInstruction();
            }
            if (system.DebuggerEnabled()) {
                system.GetDebugger().NotifyThreadStopped(current_thread);
            } else {
                LogBacktrace();
            }
            current_thread->RequestSuspend(SuspendType::Debug);
            break;
        }

        // Notify the debugger and go to sleep if a watchpoint was hit.
        if (True(hr & HaltReason::DataAbort)) {
            if (system.DebuggerEnabled()) {
                system.GetDebugger().NotifyThreadWatchpoint(current_thread, *HaltedWatchpoint());
            } else {
                LogBacktrace();
            }
            current_thread->RequestSuspend(SuspendType::Debug);
            break;
        }

        // Handle syscalls and scheduling (this may change the current thread/core)
        if (True(hr & HaltReason::SupervisorCall)) {
            Kernel::Svc::Call(system, GetSvcNumber());
            break;
        }
        if (True(hr & HaltReason::BreakLoop) || !uses_wall_clock) {
            break;
        }
    }
}

void ARM_Interface::LoadWatchpointArray(const WatchpointArray* wp) {
    watchpoints = wp;
}

const Kernel::DebugWatchpoint* ARM_Interface::MatchingWatchpoint(
    u64 addr, u64 size, Kernel::DebugWatchpointType access_type) const {
    if (!watchpoints) {
        return nullptr;
    }

    const u64 start_address{addr};
    const u64 end_address{addr + size};

    for (size_t i = 0; i < Core::Hardware::NUM_WATCHPOINTS; i++) {
        const auto& watch{(*watchpoints)[i]};

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
