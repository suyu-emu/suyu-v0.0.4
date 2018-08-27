// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <dynarmic/A64/a64.h>
#include <dynarmic/A64/exclusive_monitor.h>
#include "common/common_types.h"
#include "core/arm/arm_interface.h"
#include "core/arm/exclusive_monitor.h"
#include "core/arm/unicorn/arm_unicorn.h"

namespace Core {

class ARM_Dynarmic_Callbacks;
class DynarmicExclusiveMonitor;

class ARM_Dynarmic final : public ARM_Interface {
public:
    ARM_Dynarmic(std::shared_ptr<ExclusiveMonitor> exclusive_monitor, size_t core_index);
    ~ARM_Dynarmic();

    void MapBackingMemory(VAddr address, size_t size, u8* memory,
                          Kernel::VMAPermission perms) override;
    void UnmapMemory(u64 address, size_t size) override;
    void SetPC(u64 pc) override;
    u64 GetPC() const override;
    u64 GetReg(int index) const override;
    void SetReg(int index, u64 value) override;
    u128 GetExtReg(int index) const override;
    void SetExtReg(int index, u128 value) override;
    u32 GetVFPReg(int index) const override;
    void SetVFPReg(int index, u32 value) override;
    u32 GetCPSR() const override;
    void Run() override;
    void Step() override;
    void SetCPSR(u32 cpsr) override;
    VAddr GetTlsAddress() const override;
    void SetTlsAddress(VAddr address) override;
    void SetTPIDR_EL0(u64 value) override;
    u64 GetTPIDR_EL0() const override;

    void SaveContext(ThreadContext& ctx) override;
    void LoadContext(const ThreadContext& ctx) override;

    void PrepareReschedule() override;
    void ClearExclusiveState() override;

    void ClearInstructionCache() override;
    void PageTableChanged() override;

private:
    std::unique_ptr<Dynarmic::A64::Jit> MakeJit() const;

    friend class ARM_Dynarmic_Callbacks;
    std::unique_ptr<ARM_Dynarmic_Callbacks> cb;
    std::unique_ptr<Dynarmic::A64::Jit> jit;
    ARM_Unicorn inner_unicorn;

    size_t core_index;
    std::shared_ptr<DynarmicExclusiveMonitor> exclusive_monitor;

    Memory::PageTable* current_page_table = nullptr;
};

class DynarmicExclusiveMonitor final : public ExclusiveMonitor {
public:
    explicit DynarmicExclusiveMonitor(size_t core_count);
    ~DynarmicExclusiveMonitor();

    void SetExclusive(size_t core_index, VAddr addr) override;
    void ClearExclusive() override;

    bool ExclusiveWrite8(size_t core_index, VAddr vaddr, u8 value) override;
    bool ExclusiveWrite16(size_t core_index, VAddr vaddr, u16 value) override;
    bool ExclusiveWrite32(size_t core_index, VAddr vaddr, u32 value) override;
    bool ExclusiveWrite64(size_t core_index, VAddr vaddr, u64 value) override;
    bool ExclusiveWrite128(size_t core_index, VAddr vaddr, u128 value) override;

private:
    friend class ARM_Dynarmic;
    Dynarmic::A64::ExclusiveMonitor monitor;
};

} // namespace Core
