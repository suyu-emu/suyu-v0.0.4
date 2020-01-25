// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <map>
#include <memory>
#include <thread>

namespace Core {

class Cpu;
class System;

class CpuCoreManager {
public:
    explicit CpuCoreManager(System& system);
    CpuCoreManager(const CpuCoreManager&) = delete;
    CpuCoreManager(CpuCoreManager&&) = delete;

    ~CpuCoreManager();

    CpuCoreManager& operator=(const CpuCoreManager&) = delete;
    CpuCoreManager& operator=(CpuCoreManager&&) = delete;

    void Initialize();
    void Shutdown();

    Cpu& GetCore(std::size_t index);
    const Cpu& GetCore(std::size_t index) const;

    Cpu& GetCurrentCore();
    const Cpu& GetCurrentCore() const;

    std::size_t GetCurrentCoreIndex() const {
        return active_core;
    }

    void RunLoop(bool tight_loop);

private:
    static constexpr std::size_t NUM_CPU_CORES = 4;

    std::array<std::unique_ptr<Cpu>, NUM_CPU_CORES> cores;
    std::size_t active_core{}; ///< Active core, only used in single thread mode

    System& system;
};

} // namespace Core
