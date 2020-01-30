// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>

namespace Core {

class CoreManager;
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

    CoreManager& GetCoreManager(std::size_t index);
    const CoreManager& GetCoreManager(std::size_t index) const;

    CoreManager& GetCurrentCoreManager();
    const CoreManager& GetCurrentCoreManager() const;

    std::size_t GetActiveCoreIndex() const {
        return active_core;
    }

    void RunLoop(bool tight_loop);

private:
    static constexpr std::size_t NUM_CPU_CORES = 4;

    std::array<std::unique_ptr<CoreManager>, NUM_CPU_CORES> core_managers;
    std::size_t active_core{}; ///< Active core, only used in single thread mode

    System& system;
};

} // namespace Core
