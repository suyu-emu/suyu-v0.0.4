// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <condition_variable>
#include <mutex>

#include "common/logging/log.h"
#ifdef ARCHITECTURE_x86_64
#include "core/arm/dynarmic/arm_dynarmic.h"
#endif
#include "core/arm/exclusive_monitor.h"
#include "core/arm/unicorn/arm_unicorn.h"
#include "core/core.h"
#include "core/core_manager.h"
#include "core/core_timing.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/physical_core.h"
#include "core/hle/kernel/scheduler.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/lock.h"
#include "core/settings.h"

namespace Core {

CoreManager::CoreManager(System& system, std::size_t core_index)
    : global_scheduler{system.GlobalScheduler()}, physical_core{system.Kernel().PhysicalCore(
                                                      core_index)},
      core_timing{system.CoreTiming()}, core_index{core_index} {}

CoreManager::~CoreManager() = default;

void CoreManager::RunLoop(bool tight_loop) {
    Reschedule();

    // If we don't have a currently active thread then don't execute instructions,
    // instead advance to the next event and try to yield to the next thread
    if (Kernel::GetCurrentThread() == nullptr) {
        LOG_TRACE(Core, "Core-{} idling", core_index);
        core_timing.Idle();
    } else {
        if (tight_loop) {
            physical_core.Run();
        } else {
            physical_core.Step();
        }
    }
    core_timing.Advance();

    Reschedule();
}

void CoreManager::SingleStep() {
    return RunLoop(false);
}

void CoreManager::PrepareReschedule() {
    physical_core.Stop();
}

void CoreManager::Reschedule() {
    // Lock the global kernel mutex when we manipulate the HLE state
    std::lock_guard lock(HLE::g_hle_lock);

    global_scheduler.SelectThread(core_index);

    physical_core.Scheduler().TryDoContextSwitch();
}

} // namespace Core
