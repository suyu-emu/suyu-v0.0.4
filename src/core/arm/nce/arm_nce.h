// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

#include "core/arm/arm_interface.h"
#include "core/arm/nce/guest_context.h"

namespace Core::Memory {
class Memory;
}

namespace Core {

class System;

class ARM_NCE final : public ARM_Interface {
public:
    ARM_NCE(System& system_, bool uses_wall_clock_, std::size_t core_index_);

    ~ARM_NCE() override;

    void Initialize() override;
    void SetPC(u64 pc) override;
    u64 GetPC() const override;
    u64 GetSP() const override;
    u64 GetReg(int index) const override;
    void SetReg(int index, u64 value) override;
    u128 GetVectorReg(int index) const override;
    void SetVectorReg(int index, u128 value) override;

    u32 GetPSTATE() const override;
    void SetPSTATE(u32 pstate) override;
    u64 GetTlsAddress() const override;
    void SetTlsAddress(u64 address) override;
    void SetTPIDR_EL0(u64 value) override;
    u64 GetTPIDR_EL0() const override;

    Architecture GetArchitecture() const override {
        return Architecture::Aarch64;
    }

    void SaveContext(ThreadContext32& ctx) const override {}
    void SaveContext(ThreadContext64& ctx) const override;
    void LoadContext(const ThreadContext32& ctx) override {}
    void LoadContext(const ThreadContext64& ctx) override;

    void SignalInterrupt() override;
    void ClearInterrupt() override;
    void ClearExclusiveState() override;
    void ClearInstructionCache() override;
    void InvalidateCacheRange(u64 addr, std::size_t size) override;
    void PageTableChanged(Common::PageTable& new_page_table,
                          std::size_t new_address_space_size_in_bits) override;

protected:
    HaltReason RunJit() override;
    HaltReason StepJit() override;

    u32 GetSvcNumber() const override;

    const Kernel::DebugWatchpoint* HaltedWatchpoint() const override {
        return nullptr;
    }

    void RewindBreakpointInstruction() override {}

private:
    // Assembly definitions.
    static HaltReason ReturnToRunCodeByTrampoline(void* tpidr, GuestContext* ctx,
                                                  u64 trampoline_addr);
    static HaltReason ReturnToRunCodeByExceptionLevelChange(int tid, void* tpidr);

    static void ReturnToRunCodeByExceptionLevelChangeSignalHandler(int sig, void* info,
                                                                   void* raw_context);
    static void BreakFromRunCodeSignalHandler(int sig, void* info, void* raw_context);
    static void GuestFaultSignalHandler(int sig, void* info, void* raw_context);

    static void LockThreadParameters(void* tpidr);
    static void UnlockThreadParameters(void* tpidr);

private:
    // C++ implementation functions for assembly definitions.
    static void* RestoreGuestContext(void* raw_context);
    static void SaveGuestContext(GuestContext* ctx, void* raw_context);
    static bool HandleGuestFault(GuestContext* ctx, void* info, void* raw_context);
    static void HandleHostFault(int sig, void* info, void* raw_context);

public:
    // Members set on initialization.
    std::size_t core_index{};
    pid_t thread_id{-1};

    // Core context.
    GuestContext guest_ctx;

    // Thread and invalidation info.
    std::mutex lock;
    Kernel::KThread* running_thread{};
};

} // namespace Core
