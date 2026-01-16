// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <dynarmic/interface/A32/a32.h>

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

class ArmDynarmic32;
class DynarmicCP15;
class System;

class DynarmicCallbacks32 : public Dynarmic::A32::UserCallbacks {
public:
    explicit DynarmicCallbacks32(ArmDynarmic32& parent, Kernel::KProcess* process);
    u8 MemoryRead8(u32 vaddr) override;
    u16 MemoryRead16(u32 vaddr) override;
    u32 MemoryRead32(u32 vaddr) override;
    u64 MemoryRead64(u32 vaddr) override;
    std::optional<u32> MemoryReadCode(u32 vaddr) override;
    void MemoryWrite8(u32 vaddr, u8 value) override;
    void MemoryWrite16(u32 vaddr, u16 value) override;
    void MemoryWrite32(u32 vaddr, u32 value) override;
    void MemoryWrite64(u32 vaddr, u64 value) override;
    bool MemoryWriteExclusive8(u32 vaddr, u8 value, u8 expected) override;
    bool MemoryWriteExclusive16(u32 vaddr, u16 value, u16 expected) override;
    bool MemoryWriteExclusive32(u32 vaddr, u32 value, u32 expected) override;
    bool MemoryWriteExclusive64(u32 vaddr, u64 value, u64 expected) override;
    void InterpreterFallback(u32 pc, std::size_t num_instructions) override;
    void ExceptionRaised(u32 pc, Dynarmic::A32::Exception exception) override;
    void CallSVC(u32 swi) override;
    void AddTicks(u64 ticks) override;
    u64 GetTicksRemaining() override;
    bool CheckMemoryAccess(u64 addr, u64 size, Kernel::DebugWatchpointType type);
    void ReturnException(u32 pc, Dynarmic::HaltReason hr);
    ArmDynarmic32& m_parent;
    Core::Memory::Memory& m_memory;
    Kernel::KProcess* m_process{};
    const bool m_debugger_enabled{};
    const bool m_check_memory_access{};
};

class ArmDynarmic32 final : public ArmInterface {
public:
    ArmDynarmic32(System& system, bool uses_wall_clock, Kernel::KProcess* process, DynarmicExclusiveMonitor& exclusive_monitor, std::size_t core_index);
    ~ArmDynarmic32() override;

    Architecture GetArchitecture() const override {
        return Architecture::AArch32;
    }

    bool IsInThumbMode() const;

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
    friend class DynarmicCallbacks32;
    friend class DynarmicCP15;

    void MakeJit(Common::PageTable* page_table);

    std::optional<DynarmicCallbacks32> m_cb{};
    std::shared_ptr<DynarmicCP15> m_cp15{};
    std::size_t m_core_index{};

    std::optional<Dynarmic::A32::Jit> m_jit{};

    // SVC callback
    u32 m_svc_swi{};

    // Watchpoint info
    const Kernel::DebugWatchpoint* m_halted_watchpoint{};
    Kernel::Svc::ThreadContext m_breakpoint_context{};
};

} // namespace Core
