// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <thread>

#include "common/fiber.h"
#include "common/polyfill_thread.h"
#include "common/thread.h"
#include "core/hardware_properties.h"

namespace Kernel {
class KernelCore;
}

namespace Common {
class Event;
class Fiber;
} // namespace Common

namespace Core {

class System;

class CpuManager {
public:
    explicit CpuManager(System& system_);
    CpuManager(const CpuManager&) = delete;
    CpuManager(CpuManager&&) = delete;

    ~CpuManager();

    CpuManager& operator=(const CpuManager&) = delete;
    CpuManager& operator=(CpuManager&&) = delete;

    /// Sets if emulation is multicore or single core, must be set before Initialize
    void SetMulticore(bool is_multi) {
        is_multicore = is_multi;
    }

    /// Sets if emulation is using an asynchronous GPU.
    void SetAsyncGpu(bool is_async) {
        is_async_gpu = is_async;
    }

    void OnGpuReady() {
        gpu_barrier->Sync();
    }

    void Initialize();
    void Shutdown();

    std::function<void()> GetGuestActivateFunc(Kernel::KernelCore& kernel) {
        return [this, &kernel] { GuestActivate(kernel); };
    }
    std::function<void()> GetGuestThreadFunc(Kernel::KernelCore& kernel) {
        return [this, &kernel] { GuestThreadFunction(kernel); };
    }
    std::function<void()> GetIdleThreadStartFunc(Kernel::KernelCore& kernel) {
        return [this, &kernel] { IdleThreadFunction(kernel); };
    }
    std::function<void()> GetShutdownThreadStartFunc(Kernel::KernelCore& kernel) {
        return [this, &kernel] { ShutdownThreadFunction(kernel); };
    }

    void PreemptSingleCore(Kernel::KernelCore& kernel, bool from_running_environment = true);

    std::size_t CurrentCore() const {
        return current_core.load();
    }

private:
    void GuestThreadFunction(Kernel::KernelCore& kernel);
    void IdleThreadFunction(Kernel::KernelCore& kernel);
    void ShutdownThreadFunction(Kernel::KernelCore& kernel);

    void MultiCoreRunGuestThread(Kernel::KernelCore& kernel);
    void MultiCoreRunIdleThread(Kernel::KernelCore& kernel);

    void SingleCoreRunGuestThread(Kernel::KernelCore& kernel);
    void SingleCoreRunIdleThread(Kernel::KernelCore& kernel);

    void GuestActivate(Kernel::KernelCore& kernel);
    void HandleInterrupt(Kernel::KernelCore& kernel);
    void ShutdownThread(Kernel::KernelCore& kernel);
    void RunThread(std::stop_token stop_token, std::size_t core);

    static constexpr std::size_t max_cycle_runs = 5;

    std::optional<Common::Barrier> gpu_barrier{};
    struct CoreData {
        std::shared_ptr<Common::Fiber> host_context;
        std::jthread host_thread;
    };
    std::array<CoreData, Core::Hardware::NUM_CPU_CORES> core_data{};
    Core::System& system;
    std::atomic<std::size_t> current_core{};
    std::size_t idle_count{};
    std::size_t num_cores{};
    bool is_async_gpu{};
    bool is_multicore{};
};

} // namespace Core
