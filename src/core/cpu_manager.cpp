// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/fiber.h"
#include "common/microprofile.h"
#include "common/thread.h"
#include "core/arm/exclusive_monitor.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/cpu_manager.h"
#include "core/gdbstub/gdbstub.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/physical_core.h"
#include "core/hle/kernel/scheduler.h"
#include "core/hle/kernel/thread.h"
#include "video_core/gpu.h"

namespace Core {

CpuManager::CpuManager(System& system) : system{system} {}
CpuManager::~CpuManager() = default;

void CpuManager::ThreadStart(CpuManager& cpu_manager, std::size_t core) {
    cpu_manager.RunThread(core);
}

void CpuManager::Initialize() {
    running_mode = true;
    if (is_multicore) {
        for (std::size_t core = 0; core < Core::Hardware::NUM_CPU_CORES; core++) {
            core_data[core].host_thread =
                std::make_unique<std::thread>(ThreadStart, std::ref(*this), core);
        }
    } else {
        core_data[0].host_thread = std::make_unique<std::thread>(ThreadStart, std::ref(*this), 0);
    }
}

void CpuManager::Shutdown() {
    running_mode = false;
    Pause(false);
    if (is_multicore) {
        for (std::size_t core = 0; core < Core::Hardware::NUM_CPU_CORES; core++) {
            core_data[core].host_thread->join();
            core_data[core].host_thread.reset();
        }
    } else {
        core_data[0].host_thread->join();
        core_data[0].host_thread.reset();
    }
}

std::function<void(void*)> CpuManager::GetGuestThreadStartFunc() {
    return GuestThreadFunction;
}

std::function<void(void*)> CpuManager::GetIdleThreadStartFunc() {
    return IdleThreadFunction;
}

std::function<void(void*)> CpuManager::GetSuspendThreadStartFunc() {
    return SuspendThreadFunction;
}

void CpuManager::GuestThreadFunction(void* cpu_manager_) {
    CpuManager* cpu_manager = static_cast<CpuManager*>(cpu_manager_);
    if (cpu_manager->is_multicore) {
        cpu_manager->MultiCoreRunGuestThread();
    } else {
        cpu_manager->SingleCoreRunGuestThread();
    }
}

void CpuManager::GuestRewindFunction(void* cpu_manager_) {
    CpuManager* cpu_manager = static_cast<CpuManager*>(cpu_manager_);
    if (cpu_manager->is_multicore) {
        cpu_manager->MultiCoreRunGuestLoop();
    } else {
        cpu_manager->SingleCoreRunGuestLoop();
    }
}

void CpuManager::IdleThreadFunction(void* cpu_manager_) {
    CpuManager* cpu_manager = static_cast<CpuManager*>(cpu_manager_);
    if (cpu_manager->is_multicore) {
        cpu_manager->MultiCoreRunIdleThread();
    } else {
        cpu_manager->SingleCoreRunIdleThread();
    }
}

void CpuManager::SuspendThreadFunction(void* cpu_manager_) {
    CpuManager* cpu_manager = static_cast<CpuManager*>(cpu_manager_);
    if (cpu_manager->is_multicore) {
        cpu_manager->MultiCoreRunSuspendThread();
    } else {
        cpu_manager->SingleCoreRunSuspendThread();
    }
}

void* CpuManager::GetStartFuncParamater() {
    return static_cast<void*>(this);
}

///////////////////////////////////////////////////////////////////////////////
///                             MultiCore                                   ///
///////////////////////////////////////////////////////////////////////////////

void CpuManager::MultiCoreRunGuestThread() {
    auto& kernel = system.Kernel();
    {
        auto& sched = kernel.CurrentScheduler();
        sched.OnThreadStart();
    }
    MultiCoreRunGuestLoop();
}

void CpuManager::MultiCoreRunGuestLoop() {
    auto& kernel = system.Kernel();
    auto* thread = kernel.CurrentScheduler().GetCurrentThread();
    while (true) {
        auto* physical_core = &kernel.CurrentPhysicalCore();
        auto& arm_interface = thread->ArmInterface();
        system.EnterDynarmicProfile();
        while (!physical_core->IsInterrupted()) {
            arm_interface.Run();
            physical_core = &kernel.CurrentPhysicalCore();
        }
        system.ExitDynarmicProfile();
        arm_interface.ClearExclusiveState();
        auto& scheduler = kernel.CurrentScheduler();
        scheduler.TryDoContextSwitch();
    }
}

void CpuManager::MultiCoreRunIdleThread() {
    auto& kernel = system.Kernel();
    while (true) {
        auto& physical_core = kernel.CurrentPhysicalCore();
        physical_core.Idle();
        auto& scheduler = kernel.CurrentScheduler();
        scheduler.TryDoContextSwitch();
    }
}

void CpuManager::MultiCoreRunSuspendThread() {
    auto& kernel = system.Kernel();
    {
        auto& sched = kernel.CurrentScheduler();
        sched.OnThreadStart();
    }
    while (true) {
        auto core = kernel.GetCurrentHostThreadID();
        auto& scheduler = kernel.CurrentScheduler();
        Kernel::Thread* current_thread = scheduler.GetCurrentThread();
        Common::Fiber::YieldTo(current_thread->GetHostContext(), core_data[core].host_context);
        ASSERT(scheduler.ContextSwitchPending());
        ASSERT(core == kernel.GetCurrentHostThreadID());
        scheduler.TryDoContextSwitch();
    }
}

void CpuManager::MultiCorePause(bool paused) {
    if (!paused) {
        bool all_not_barrier = false;
        while (!all_not_barrier) {
            all_not_barrier = true;
            for (std::size_t core = 0; core < Core::Hardware::NUM_CPU_CORES; core++) {
                all_not_barrier &=
                    !core_data[core].is_running.load() && core_data[core].initialized.load();
            }
        }
        for (std::size_t core = 0; core < Core::Hardware::NUM_CPU_CORES; core++) {
            core_data[core].enter_barrier->Set();
        }
        if (paused_state.load()) {
            bool all_barrier = false;
            while (!all_barrier) {
                all_barrier = true;
                for (std::size_t core = 0; core < Core::Hardware::NUM_CPU_CORES; core++) {
                    all_barrier &=
                        core_data[core].is_paused.load() && core_data[core].initialized.load();
                }
            }
            for (std::size_t core = 0; core < Core::Hardware::NUM_CPU_CORES; core++) {
                core_data[core].exit_barrier->Set();
            }
        }
    } else {
        /// Wait until all cores are paused.
        bool all_barrier = false;
        while (!all_barrier) {
            all_barrier = true;
            for (std::size_t core = 0; core < Core::Hardware::NUM_CPU_CORES; core++) {
                all_barrier &=
                    core_data[core].is_paused.load() && core_data[core].initialized.load();
            }
        }
        /// Don't release the barrier
    }
    paused_state = paused;
}

///////////////////////////////////////////////////////////////////////////////
///                             SingleCore                                   ///
///////////////////////////////////////////////////////////////////////////////

void CpuManager::SingleCoreRunGuestThread() {
    auto& kernel = system.Kernel();
    {
        auto& sched = kernel.CurrentScheduler();
        sched.OnThreadStart();
    }
    SingleCoreRunGuestLoop();
}

void CpuManager::SingleCoreRunGuestLoop() {
    auto& kernel = system.Kernel();
    auto* thread = kernel.CurrentScheduler().GetCurrentThread();
    while (true) {
        auto* physical_core = &kernel.CurrentPhysicalCore();
        auto& arm_interface = thread->ArmInterface();
        system.EnterDynarmicProfile();
        if (!physical_core->IsInterrupted()) {
            arm_interface.Run();
            physical_core = &kernel.CurrentPhysicalCore();
        }
        system.ExitDynarmicProfile();
        thread->SetPhantomMode(true);
        system.CoreTiming().Advance();
        thread->SetPhantomMode(false);
        arm_interface.ClearExclusiveState();
        PreemptSingleCore();
        auto& scheduler = kernel.Scheduler(current_core);
        scheduler.TryDoContextSwitch();
    }
}

void CpuManager::SingleCoreRunIdleThread() {
    auto& kernel = system.Kernel();
    while (true) {
        auto& physical_core = kernel.CurrentPhysicalCore();
        PreemptSingleCore(false);
        system.CoreTiming().AddTicks(1000U);
        idle_count++;
        auto& scheduler = physical_core.Scheduler();
        scheduler.TryDoContextSwitch();
    }
}

void CpuManager::SingleCoreRunSuspendThread() {
    auto& kernel = system.Kernel();
    {
        auto& sched = kernel.CurrentScheduler();
        sched.OnThreadStart();
    }
    while (true) {
        auto core = kernel.GetCurrentHostThreadID();
        auto& scheduler = kernel.CurrentScheduler();
        Kernel::Thread* current_thread = scheduler.GetCurrentThread();
        Common::Fiber::YieldTo(current_thread->GetHostContext(), core_data[0].host_context);
        ASSERT(scheduler.ContextSwitchPending());
        ASSERT(core == kernel.GetCurrentHostThreadID());
        scheduler.TryDoContextSwitch();
    }
}

void CpuManager::PreemptSingleCore(bool from_running_enviroment) {
    std::size_t old_core = current_core;
    auto& scheduler = system.Kernel().Scheduler(old_core);
    Kernel::Thread* current_thread = scheduler.GetCurrentThread();
    if (idle_count >= 4 || from_running_enviroment) {
        if (!from_running_enviroment) {
            system.CoreTiming().Idle();
            idle_count = 0;
        }
        current_thread->SetPhantomMode(true);
        system.CoreTiming().Advance();
        current_thread->SetPhantomMode(false);
    }
    current_core.store((current_core + 1) % Core::Hardware::NUM_CPU_CORES);
    system.CoreTiming().ResetTicks();
    scheduler.Unload();
    auto& next_scheduler = system.Kernel().Scheduler(current_core);
    Common::Fiber::YieldTo(current_thread->GetHostContext(), next_scheduler.ControlContext());
    /// May have changed scheduler
    auto& current_scheduler = system.Kernel().Scheduler(current_core);
    current_scheduler.Reload();
    auto* currrent_thread2 = current_scheduler.GetCurrentThread();
    if (!currrent_thread2->IsIdleThread()) {
        idle_count = 0;
    }
}

void CpuManager::SingleCorePause(bool paused) {
    if (!paused) {
        bool all_not_barrier = false;
        while (!all_not_barrier) {
            all_not_barrier = !core_data[0].is_running.load() && core_data[0].initialized.load();
        }
        core_data[0].enter_barrier->Set();
        if (paused_state.load()) {
            bool all_barrier = false;
            while (!all_barrier) {
                all_barrier = core_data[0].is_paused.load() && core_data[0].initialized.load();
            }
            core_data[0].exit_barrier->Set();
        }
    } else {
        /// Wait until all cores are paused.
        bool all_barrier = false;
        while (!all_barrier) {
            all_barrier = core_data[0].is_paused.load() && core_data[0].initialized.load();
        }
        /// Don't release the barrier
    }
    paused_state = paused;
}

void CpuManager::Pause(bool paused) {
    if (is_multicore) {
        MultiCorePause(paused);
    } else {
        SingleCorePause(paused);
    }
}

void CpuManager::RunThread(std::size_t core) {
    /// Initialization
    system.RegisterCoreThread(core);
    std::string name;
    if (is_multicore) {
        name = "yuzu:CoreCPUThread_" + std::to_string(core);
    } else {
        name = "yuzu:CPUThread";
    }
    MicroProfileOnThreadCreate(name.c_str());
    Common::SetCurrentThreadName(name.c_str());
    Common::SetCurrentThreadPriority(Common::ThreadPriority::High);
    auto& data = core_data[core];
    data.enter_barrier = std::make_unique<Common::Event>();
    data.exit_barrier = std::make_unique<Common::Event>();
    data.host_context = Common::Fiber::ThreadToFiber();
    data.is_running = false;
    data.initialized = true;
    const bool sc_sync = !is_async_gpu && !is_multicore;
    bool sc_sync_first_use = sc_sync;
    /// Running
    while (running_mode) {
        data.is_running = false;
        data.enter_barrier->Wait();
        if (sc_sync_first_use) {
            system.GPU().ObtainContext();
            sc_sync_first_use = false;
        }
        auto& scheduler = system.Kernel().CurrentScheduler();
        Kernel::Thread* current_thread = scheduler.GetCurrentThread();
        data.is_running = true;
        Common::Fiber::YieldTo(data.host_context, current_thread->GetHostContext());
        data.is_running = false;
        data.is_paused = true;
        data.exit_barrier->Wait();
        data.is_paused = false;
    }
    /// Time to cleanup
    data.host_context->Exit();
    data.enter_barrier.reset();
    data.exit_barrier.reset();
    data.initialized = false;
}

} // namespace Core
