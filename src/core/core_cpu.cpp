// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <condition_variable>
#include <mutex>

#include "common/logging/log.h"
#ifdef ARCHITECTURE_x86_64
#include "core/arm/dynarmic/arm_dynarmic.h"
#endif
#include "core/arm/unicorn/arm_unicorn.h"
#include "core/core_cpu.h"
#include "core/core_timing.h"
#include "core/hle/kernel/scheduler.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/lock.h"
#include "core/settings.h"

namespace Core {

void CpuBarrier::NotifyEnd() {
    std::unique_lock<std::mutex> lock(mutex);
    end = true;
    condition.notify_all();
}

bool CpuBarrier::Rendezvous() {
    if (!Settings::values.use_multi_core) {
        // Meaningless when running in single-core mode
        return true;
    }

    if (!end) {
        std::unique_lock<std::mutex> lock(mutex);

        --cores_waiting;
        if (!cores_waiting) {
            cores_waiting = NUM_CPU_CORES;
            condition.notify_all();
            return true;
        }

        condition.wait(lock);
        return true;
    }

    return false;
}

Cpu::Cpu(std::shared_ptr<ExclusiveMonitor> exclusive_monitor,
         std::shared_ptr<CpuBarrier> cpu_barrier, std::size_t core_index)
    : cpu_barrier{std::move(cpu_barrier)}, core_index{core_index} {

    if (Settings::values.use_cpu_jit) {
#ifdef ARCHITECTURE_x86_64
        arm_interface = std::make_shared<ARM_Dynarmic>(exclusive_monitor, core_index);
#else
        arm_interface = std::make_shared<ARM_Unicorn>();
        LOG_WARNING(Core, "CPU JIT requested, but Dynarmic not available");
#endif
    } else {
        arm_interface = std::make_shared<ARM_Unicorn>();
    }

    scheduler = std::make_shared<Kernel::Scheduler>(arm_interface.get());
}

std::shared_ptr<ExclusiveMonitor> Cpu::MakeExclusiveMonitor(std::size_t num_cores) {
    if (Settings::values.use_cpu_jit) {
#ifdef ARCHITECTURE_x86_64
        return std::make_shared<DynarmicExclusiveMonitor>(num_cores);
#else
        return nullptr; // TODO(merry): Passthrough exclusive monitor
#endif
    } else {
        return nullptr; // TODO(merry): Passthrough exclusive monitor
    }
}

void Cpu::RunLoop(bool tight_loop) {
    // Wait for all other CPU cores to complete the previous slice, such that they run in lock-step
    if (!cpu_barrier->Rendezvous()) {
        // If rendezvous failed, session has been killed
        return;
    }

    // If we don't have a currently active thread then don't execute instructions,
    // instead advance to the next event and try to yield to the next thread
    if (Kernel::GetCurrentThread() == nullptr) {
        LOG_TRACE(Core, "Core-{} idling", core_index);

        if (IsMainCore()) {
            // TODO(Subv): Only let CoreTiming idle if all 4 cores are idling.
            CoreTiming::Idle();
            CoreTiming::Advance();
        }

        PrepareReschedule();
    } else {
        if (IsMainCore()) {
            CoreTiming::Advance();
        }

        if (tight_loop) {
            arm_interface->Run();
        } else {
            arm_interface->Step();
        }
    }

    Reschedule();
}

void Cpu::SingleStep() {
    return RunLoop(false);
}

void Cpu::PrepareReschedule() {
    arm_interface->PrepareReschedule();
    reschedule_pending = true;
}

void Cpu::Reschedule() {
    if (!reschedule_pending) {
        return;
    }

    reschedule_pending = false;
    // Lock the global kernel mutex when we manipulate the HLE state
    std::lock_guard<std::recursive_mutex> lock(HLE::g_hle_lock);
    scheduler->Reschedule();
}

} // namespace Core
