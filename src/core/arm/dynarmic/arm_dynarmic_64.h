// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <memory>
#include <unordered_map>

#include <dynarmic/interface/A64/a64.h>
#include "common/common_types.h"
#include "common/hash.h"
#include "core/arm/arm_interface.h"
#include "core/arm/dynarmic/dynarmic_exclusive_monitor.h"

namespace Core::Memory {
class Memory;
}

namespace Kernel {
enum class DebugWatchpointType : u8;
class KPRocess;
}

namespace Core {

class ArmDynarmic64;
class DynarmicExclusiveMonitor;
class System;

class DynarmicCallbacks64 : public Dynarmic::A64::UserCallbacks {
public:
    explicit DynarmicCallbacks64(ArmDynarmic64& parent, Kernel::KProcess* process);

    u8 MemoryRead8(u64 vaddr) override;
    u16 MemoryRead16(u64 vaddr) override;
    u32 MemoryRead32(u64 vaddr) override;
    u64 MemoryRead64(u64 vaddr) override;
    Dynarmic::A64::Vector MemoryRead128(u64 vaddr) override;
    std::optional<u32> MemoryReadCode(u64 vaddr) override;
    void MemoryWrite8(u64 vaddr, u8 value) override;
    void MemoryWrite16(u64 vaddr, u16 value) override;
    void MemoryWrite32(u64 vaddr, u32 value) override;
    void MemoryWrite64(u64 vaddr, u64 value) override;
    void MemoryWrite128(u64 vaddr, Dynarmic::A64::Vector value) override;
    bool MemoryWriteExclusive8(u64 vaddr, std::uint8_t value, std::uint8_t expected) override;
    bool MemoryWriteExclusive16(u64 vaddr, std::uint16_t value, std::uint16_t expected) override;
    bool MemoryWriteExclusive32(u64 vaddr, std::uint32_t value, std::uint32_t expected) override;
    bool MemoryWriteExclusive64(u64 vaddr, std::uint64_t value, std::uint64_t expected) override;
    bool MemoryWriteExclusive128(u64 vaddr, Dynarmic::A64::Vector value, Dynarmic::A64::Vector expected) override;
    void InterpreterFallback(u64 pc, std::size_t num_instructions) override;
    void InstructionCacheOperationRaised(Dynarmic::A64::InstructionCacheOperation op, u64 value) override;
    void ExceptionRaised(u64 pc, Dynarmic::A64::Exception exception) override;
    void CallSVC(u32 svc) override;
    void AddTicks(u64 ticks) override;
    u64 GetTicksRemaining() override;
    u64 GetCNTPCT() override;
    bool CheckMemoryAccess(u64 addr, u64 size, Kernel::DebugWatchpointType type);
    void ReturnException(u64 pc, Dynarmic::HaltReason hr);

    ArmDynarmic64& m_parent;
    Core::Memory::Memory& m_memory;
    u64 m_tpidrro_el0{};
    u64 m_tpidr_el0{};
    Kernel::KProcess* m_process{};
    const bool m_debugger_enabled{};
    const bool m_check_memory_access{};
    static constexpr u64 MinimumRunCycles = 10000U;
};

class ArmDynarmic64 final : public ArmInterface {
public:
    ArmDynarmic64(System& system, bool uses_wall_clock, Kernel::KProcess* process,
                  DynarmicExclusiveMonitor& exclusive_monitor, std::size_t core_index);
    ~ArmDynarmic64() override;

    Architecture GetArchitecture() const override {
        return Architecture::AArch64;
    }

    HaltReason RunThread(Kernel::KThread* thread) override;
    HaltReason StepThread(Kernel::KThread* thread) override;

    void GetContext(Kernel::Svc::ThreadContext& ctx) const override;
    void SetContext(const Kernel::Svc::ThreadContext& ctx) override;
    void SetTpidrroEl0(u64 value) override;

    void GetSvcArguments(std::span<uint64_t, 8> args) const override;
    void SetSvcArguments(std::span<const uint64_t, 8> args) override;
    u32 GetSvcNumber() const override;

    void SignalInterrupt(Kernel::KThread* thread) override;
    void ClearInstructionCache() override;
    void InvalidateCacheRange(u64 addr, std::size_t size) override;

protected:
    const Kernel::DebugWatchpoint* HaltedWatchpoint() const override;
    void RewindBreakpointInstruction() override;

private:
    System& m_system;
    DynarmicExclusiveMonitor& m_exclusive_monitor;

private:
    friend class DynarmicCallbacks64;

    void MakeJit(Common::PageTable* page_table, std::size_t address_space_bits);
    std::optional<DynarmicCallbacks64> m_cb{};
    std::size_t m_core_index{};

    std::optional<Dynarmic::A64::Jit> m_jit{};

    // SVC callback
    u32 m_svc{};

    // Watchpoint info
    const Kernel::DebugWatchpoint* m_halted_watchpoint{};
    Kernel::Svc::ThreadContext m_breakpoint_context{};
};

} // namespace Core
