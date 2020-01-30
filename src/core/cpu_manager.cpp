// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/arm/exclusive_monitor.h"
#include "core/core.h"
#include "core/core_manager.h"
#include "core/core_timing.h"
#include "core/cpu_manager.h"
#include "core/gdbstub/gdbstub.h"

namespace Core {

CpuManager::CpuManager(System& system) : system{system} {}
CpuManager::~CpuManager() = default;

void CpuManager::Initialize() {
    for (std::size_t index = 0; index < core_managers.size(); ++index) {
        core_managers[index] = std::make_unique<CoreManager>(system, index);
    }
}

void CpuManager::Shutdown() {
    for (auto& cpu_core : core_managers) {
        cpu_core.reset();
    }
}

CoreManager& CpuManager::GetCoreManager(std::size_t index) {
    return *core_managers.at(index);
}

const CoreManager& CpuManager::GetCoreManager(std::size_t index) const {
    return *core_managers.at(index);
}

CoreManager& CpuManager::GetCurrentCoreManager() {
    // Otherwise, use single-threaded mode active_core variable
    return *core_managers[active_core];
}

const CoreManager& CpuManager::GetCurrentCoreManager() const {
    // Otherwise, use single-threaded mode active_core variable
    return *core_managers[active_core];
}

void CpuManager::RunLoop(bool tight_loop) {
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
                core_managers[active_core]->RunLoop(tight_loop);
            }
            keep_running |= core_timing.CanCurrentContextRun();
        }
    } while (keep_running);

    if (GDBStub::IsServerEnabled()) {
        GDBStub::SetCpuStepFlag(false);
    }
}

} // namespace Core
