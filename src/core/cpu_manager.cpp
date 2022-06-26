// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/fiber.h"
#include "common/microprofile.h"
#include "common/scope_exit.h"
#include "common/thread.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/cpu_manager.h"
#include "core/hle/kernel/k_interrupt_manager.h"
#include "core/hle/kernel/k_scheduler.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/physical_core.h"
#include "video_core/gpu.h"

namespace Core {

CpuManager::CpuManager(System& system_) : system{system_} {}
CpuManager::~CpuManager() = default;

void CpuManager::ThreadStart(std::stop_token stop_token, CpuManager& cpu_manager,
                             std::size_t core) {
    cpu_manager.RunThread(core);
}

void CpuManager::Initialize() {
    num_cores = is_multicore ? Core::Hardware::NUM_CPU_CORES : 1;
    gpu_barrier = std::make_unique<Common::Barrier>(num_cores + 1);

    for (std::size_t core = 0; core < num_cores; core++) {
        core_data[core].host_thread = std::jthread(ThreadStart, std::ref(*this), core);
    }
}

void CpuManager::Shutdown() {
    for (std::size_t core = 0; core < num_cores; core++) {
        if (core_data[core].host_thread.joinable()) {
            core_data[core].host_thread.join();
        }
    }
}

void CpuManager::GuestActivateFunction() {
    if (is_multicore) {
        MultiCoreGuestActivate();
    } else {
        SingleCoreGuestActivate();
    }
}

void CpuManager::GuestThreadFunction() {
    if (is_multicore) {
        MultiCoreRunGuestThread();
    } else {
        SingleCoreRunGuestThread();
    }
}

void CpuManager::ShutdownThreadFunction() {
    ShutdownThread();
}

void CpuManager::WaitForAndHandleInterrupt() {
    auto& kernel = system.Kernel();
    auto& physical_core = kernel.CurrentPhysicalCore();

    ASSERT(Kernel::GetCurrentThread(kernel).GetDisableDispatchCount() == 1);

    if (!physical_core.IsInterrupted()) {
        physical_core.Idle();
    }

    HandleInterrupt();
}

void CpuManager::HandleInterrupt() {
    auto& kernel = system.Kernel();
    auto core_index = kernel.CurrentPhysicalCoreIndex();

    Kernel::KInterruptManager::HandleInterrupt(kernel, static_cast<s32>(core_index));
}

///////////////////////////////////////////////////////////////////////////////
///                             MultiCore                                   ///
///////////////////////////////////////////////////////////////////////////////

void CpuManager::MultiCoreGuestActivate() {
    // Similar to the HorizonKernelMain callback in HOS
    auto& kernel = system.Kernel();
    auto* scheduler = kernel.CurrentScheduler();

    scheduler->Activate();
    UNREACHABLE();
}

void CpuManager::MultiCoreRunGuestThread() {
    // Similar to UserModeThreadStarter in HOS
    auto& kernel = system.Kernel();
    auto* thread = kernel.GetCurrentEmuThread();
    thread->EnableDispatch();

    MultiCoreRunGuestLoop();
}

void CpuManager::MultiCoreRunGuestLoop() {
    auto& kernel = system.Kernel();

    while (true) {
        auto* physical_core = &kernel.CurrentPhysicalCore();
        while (!physical_core->IsInterrupted()) {
            physical_core->Run();
            physical_core = &kernel.CurrentPhysicalCore();
        }

        HandleInterrupt();
    }
}

///////////////////////////////////////////////////////////////////////////////
///                             SingleCore                                   ///
///////////////////////////////////////////////////////////////////////////////

void CpuManager::SingleCoreGuestActivate() {}

void CpuManager::SingleCoreRunGuestThread() {}

void CpuManager::SingleCoreRunGuestLoop() {}

void CpuManager::PreemptSingleCore(bool from_running_enviroment) {}

void CpuManager::ShutdownThread() {
    auto& kernel = system.Kernel();
    auto* thread = kernel.GetCurrentEmuThread();
    auto core = is_multicore ? kernel.CurrentPhysicalCoreIndex() : 0;

    Common::Fiber::YieldTo(thread->GetHostContext(), *core_data[core].host_context);
    UNREACHABLE();
}

void CpuManager::RunThread(std::size_t core) {
    /// Initialization
    system.RegisterCoreThread(core);
    std::string name;
    if (is_multicore) {
        name = "yuzu:CPUCore_" + std::to_string(core);
    } else {
        name = "yuzu:CPUThread";
    }
    MicroProfileOnThreadCreate(name.c_str());
    Common::SetCurrentThreadName(name.c_str());
    Common::SetCurrentThreadPriority(Common::ThreadPriority::High);
    auto& data = core_data[core];
    data.host_context = Common::Fiber::ThreadToFiber();

    // Cleanup
    SCOPE_EXIT({
        data.host_context->Exit();
        MicroProfileOnThreadExit();
    });

    // Running
    gpu_barrier->Sync();

    if (!is_async_gpu && !is_multicore) {
        system.GPU().ObtainContext();
    }

    auto& kernel = system.Kernel();

    auto* main_thread = Kernel::KThread::Create(kernel);
    main_thread->SetName(fmt::format("MainThread:{}", core));
    ASSERT(Kernel::KThread::InitializeMainThread(system, main_thread, static_cast<s32>(core))
               .IsSuccess());

    auto* idle_thread = Kernel::KThread::Create(kernel);
    ASSERT(Kernel::KThread::InitializeIdleThread(system, idle_thread, static_cast<s32>(core))
               .IsSuccess());

    kernel.SetCurrentEmuThread(main_thread);
    kernel.CurrentScheduler()->Initialize(idle_thread);

    Common::Fiber::YieldTo(data.host_context, *main_thread->GetHostContext());
}

} // namespace Core
