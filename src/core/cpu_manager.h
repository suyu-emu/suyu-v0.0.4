// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <functional>
#include <memory>
#include <thread>
#include "core/hardware_properties.h"

namespace Common {
class Event;
class Fiber;
} // namespace Common

namespace Core {

class System;

class CpuManager {
public:
    explicit CpuManager(System& system);
    CpuManager(const CpuManager&) = delete;
    CpuManager(CpuManager&&) = delete;

    ~CpuManager();

    CpuManager& operator=(const CpuManager&) = delete;
    CpuManager& operator=(CpuManager&&) = delete;

    void Initialize();
    void Shutdown();

    void Pause(bool paused);

    std::function<void(void*)> GetGuestThreadStartFunc();
    std::function<void(void*)> GetIdleThreadStartFunc();
    std::function<void(void*)> GetSuspendThreadStartFunc();
    void* GetStartFuncParamater();

private:
    static void GuestThreadFunction(void* cpu_manager);
    static void GuestRewindFunction(void* cpu_manager);
    static void IdleThreadFunction(void* cpu_manager);
    static void SuspendThreadFunction(void* cpu_manager);

    void RunGuestThread();
    void RunGuestLoop();
    void RunIdleThread();
    void RunSuspendThread();

    static void ThreadStart(CpuManager& cpu_manager, std::size_t core);

    void RunThread(std::size_t core);

    struct CoreData {
        std::shared_ptr<Common::Fiber> host_context;
        std::unique_ptr<Common::Event> enter_barrier;
        std::unique_ptr<Common::Event> exit_barrier;
        std::atomic<bool> is_running;
        std::atomic<bool> is_paused;
        std::atomic<bool> initialized;
        std::unique_ptr<std::thread> host_thread;
    };

    std::atomic<bool> running_mode{};
    std::atomic<bool> paused_state{};

    std::array<CoreData, Core::Hardware::NUM_CPU_CORES> core_data{};

    System& system;
};

} // namespace Core
