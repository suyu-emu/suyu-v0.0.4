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

namespace Core {

CpuManager::CpuManager(System& system) : system{system} {}
CpuManager::~CpuManager() = default;

void CpuManager::ThreadStart(CpuManager& cpu_manager, std::size_t core) {
    cpu_manager.RunThread(core);
}

void CpuManager::Initialize() {
    running_mode = true;
    for (std::size_t core = 0; core < Core::Hardware::NUM_CPU_CORES; core++) {
        core_data[core].host_thread =
            std::make_unique<std::thread>(ThreadStart, std::ref(*this), core);
    }
}

void CpuManager::Shutdown() {
    running_mode = false;
    Pause(false);
    for (std::size_t core = 0; core < Core::Hardware::NUM_CPU_CORES; core++) {
        core_data[core].host_thread->join();
        core_data[core].host_thread.reset();
    }
}

void CpuManager::GuestThreadFunction(void* cpu_manager_) {
    CpuManager* cpu_manager = static_cast<CpuManager*>(cpu_manager_);
    cpu_manager->RunGuestThread();
}

void CpuManager::GuestRewindFunction(void* cpu_manager_) {
    CpuManager* cpu_manager = static_cast<CpuManager*>(cpu_manager_);
    cpu_manager->RunGuestLoop();
}

void CpuManager::IdleThreadFunction(void* cpu_manager_) {
    CpuManager* cpu_manager = static_cast<CpuManager*>(cpu_manager_);
    cpu_manager->RunIdleThread();
}

void CpuManager::SuspendThreadFunction(void* cpu_manager_) {
    CpuManager* cpu_manager = static_cast<CpuManager*>(cpu_manager_);
    cpu_manager->RunSuspendThread();
}

std::function<void(void*)> CpuManager::GetGuestThreadStartFunc() {
    return std::function<void(void*)>(GuestThreadFunction);
}

std::function<void(void*)> CpuManager::GetIdleThreadStartFunc() {
    return std::function<void(void*)>(IdleThreadFunction);
}

std::function<void(void*)> CpuManager::GetSuspendThreadStartFunc() {
    return std::function<void(void*)>(SuspendThreadFunction);
}

void* CpuManager::GetStartFuncParamater() {
    return static_cast<void*>(this);
}

void CpuManager::RunGuestThread() {
    auto& kernel = system.Kernel();
    {
        auto& sched = kernel.CurrentScheduler();
        sched.OnThreadStart();
    }
    RunGuestLoop();
}

void CpuManager::RunGuestLoop() {
    auto& kernel = system.Kernel();
    auto* thread = kernel.CurrentScheduler().GetCurrentThread();
    auto host_context = thread->GetHostContext();
    host_context->SetRewindPoint(std::function<void(void*)>(GuestRewindFunction), this);
    host_context.reset();
    while (true) {
        auto& physical_core = kernel.CurrentPhysicalCore();
        while (!physical_core.IsInterrupted()) {
            physical_core.Run();
        }
        physical_core.ClearExclusive();
        auto& scheduler = physical_core.Scheduler();
        scheduler.TryDoContextSwitch();
    }
}

void CpuManager::RunIdleThread() {
    auto& kernel = system.Kernel();
    while (true) {
        auto& physical_core = kernel.CurrentPhysicalCore();
        physical_core.Idle();
        auto& scheduler = physical_core.Scheduler();
        scheduler.TryDoContextSwitch();
    }
}

void CpuManager::RunSuspendThread() {
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

void CpuManager::Pause(bool paused) {
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

void CpuManager::RunThread(std::size_t core) {
    /// Initialization
    system.RegisterCoreThread(core);
    std::string name = "yuzu:CoreHostThread_" + std::to_string(core);
    MicroProfileOnThreadCreate(name.c_str());
    Common::SetCurrentThreadName(name.c_str());
    auto& data = core_data[core];
    data.enter_barrier = std::make_unique<Common::Event>();
    data.exit_barrier = std::make_unique<Common::Event>();
    data.host_context = Common::Fiber::ThreadToFiber();
    data.is_running = false;
    data.initialized = true;
    /// Running
    while (running_mode) {
        data.is_running = false;
        data.enter_barrier->Wait();
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
