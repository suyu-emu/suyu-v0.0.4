// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <unordered_map>

#include <dynarmic/A64/a64.h>
#include <dynarmic/A64/exclusive_monitor.h>
#include "common/common_types.h"
#include "common/hash.h"
#include "core/arm/arm_interface.h"
#include "core/arm/exclusive_monitor.h"
#include "core/arm/unicorn/arm_unicorn.h"

namespace Core::Memory {
class Memory;
}

namespace Core {

class DynarmicCallbacks64;
class DynarmicExclusiveMonitor;
class System;

class ARM_Dynarmic_64 final : public ARM_Interface {
public:
    ARM_Dynarmic_64(System& system, ExclusiveMonitor& exclusive_monitor, std::size_t core_index);
    ~ARM_Dynarmic_64() override;

    void SetPC(u64 pc) override;
    u64 GetPC() const override;
    u64 GetReg(int index) const override;
    void SetReg(int index, u64 value) override;
    u128 GetVectorReg(int index) const override;
    void SetVectorReg(int index, u128 value) override;
    u32 GetPSTATE() const override;
    void SetPSTATE(u32 pstate) override;
    void Run() override;
    void Step() override;
    VAddr GetTlsAddress() const override;
    void SetTlsAddress(VAddr address) override;
    void SetTPIDR_EL0(u64 value) override;
    u64 GetTPIDR_EL0() const override;

    void SaveContext(ThreadContext32& ctx) override {}
    void SaveContext(ThreadContext64& ctx) override;
    void LoadContext(const ThreadContext32& ctx) override {}
    void LoadContext(const ThreadContext64& ctx) override;

    void PrepareReschedule() override;
    void ClearExclusiveState() override;

    void ClearInstructionCache() override;
    void PageTableChanged(Common::PageTable& new_page_table,
                          std::size_t new_address_space_size_in_bits) override;

private:
    std::shared_ptr<Dynarmic::A64::Jit> MakeJit(Common::PageTable& page_table,
                                                std::size_t address_space_bits) const;

    using JitCacheKey = std::pair<Common::PageTable*, std::size_t>;
    using JitCacheType =
        std::unordered_map<JitCacheKey, std::shared_ptr<Dynarmic::A64::Jit>, Common::PairHash>;

    friend class DynarmicCallbacks64;
    std::unique_ptr<DynarmicCallbacks64> cb;
    JitCacheType jit_cache;
    std::shared_ptr<Dynarmic::A64::Jit> jit;
    ARM_Unicorn inner_unicorn;

    std::size_t core_index;
    DynarmicExclusiveMonitor& exclusive_monitor;
};

class DynarmicExclusiveMonitor final : public ExclusiveMonitor {
public:
    explicit DynarmicExclusiveMonitor(Memory::Memory& memory, std::size_t core_count);
    ~DynarmicExclusiveMonitor() override;

    void SetExclusive(std::size_t core_index, VAddr addr) override;
    void ClearExclusive() override;

    bool ExclusiveWrite8(std::size_t core_index, VAddr vaddr, u8 value) override;
    bool ExclusiveWrite16(std::size_t core_index, VAddr vaddr, u16 value) override;
    bool ExclusiveWrite32(std::size_t core_index, VAddr vaddr, u32 value) override;
    bool ExclusiveWrite64(std::size_t core_index, VAddr vaddr, u64 value) override;
    bool ExclusiveWrite128(std::size_t core_index, VAddr vaddr, u128 value) override;

private:
    friend class ARM_Dynarmic_64;
    Dynarmic::A64::ExclusiveMonitor monitor;
    Core::Memory::Memory& memory;
};

} // namespace Core
