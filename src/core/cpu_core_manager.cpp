// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "core/arm/exclusive_monitor.h"
#include "core/core.h"
#include "core/core_cpu.h"
#include "core/core_timing.h"
#include "core/cpu_core_manager.h"
#include "core/gdbstub/gdbstub.h"
#include "core/settings.h"

namespace Core {
namespace {
void RunCpuCore(const System& system, Cpu& cpu_state) {
    while (system.IsPoweredOn()) {
        cpu_state.RunLoop(true);
    }
}
} // Anonymous namespace

CpuCoreManager::CpuCoreManager(System& system) : system{system} {}
CpuCoreManager::~CpuCoreManager() = default;

void CpuCoreManager::Initialize() {

    for (std::size_t index = 0; index < cores.size(); ++index) {
        cores[index] = std::make_unique<Cpu>(system, index);
    }
}

void CpuCoreManager::Shutdown() {
    for (auto& cpu_core : cores) {
        cpu_core.reset();
    }
}

Cpu& CpuCoreManager::GetCore(std::size_t index) {
    return *cores.at(index);
}

const Cpu& CpuCoreManager::GetCore(std::size_t index) const {
    return *cores.at(index);
}

Cpu& CpuCoreManager::GetCurrentCore() {
    // Otherwise, use single-threaded mode active_core variable
    return *cores[active_core];
}

const Cpu& CpuCoreManager::GetCurrentCore() const {
    // Otherwise, use single-threaded mode active_core variable
    return *cores[active_core];
}

void CpuCoreManager::RunLoop(bool tight_loop) {
    if (GDBStub::IsServerEnabled()) {
        GDBStub::HandlePacket();

        // If the loop is halted and we want to step, use a tiny (1) number of instructions to
        // execute. Otherwise, get out of the loop function.
        if (GDBStub::GetCpuHaltFlag()) {
            if (GDBStub::GetCpuStepFlag()) {
                tight_loop = false;
            } else {
                return;
            }
        }
    }

    auto& core_timing = system.CoreTiming();
    core_timing.ResetRun();
    bool keep_running{};
    do {
        keep_running = false;
        for (active_core = 0; active_core < NUM_CPU_CORES; ++active_core) {
            core_timing.SwitchContext(active_core);
            if (core_timing.CanCurrentContextRun()) {
                cores[active_core]->RunLoop(tight_loop);
            }
            keep_running |= core_timing.CanCurrentContextRun();
        }
    } while (keep_running);

    if (GDBStub::IsServerEnabled()) {
        GDBStub::SetCpuStepFlag(false);
    }
}

} // namespace Core
