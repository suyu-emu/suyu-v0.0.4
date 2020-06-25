// Copyright 2020 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cinttypes>
#include <memory>
#include <dynarmic/A32/a32.h>
#include <dynarmic/A32/config.h>
#include <dynarmic/A32/context.h>
#include "common/microprofile.h"
#include "core/arm/dynarmic/arm_dynarmic_32.h"
#include "core/arm/dynarmic/arm_dynarmic_64.h"
#include "core/arm/dynarmic/arm_dynarmic_cp15.h"
#include "core/core.h"
#include "core/core_manager.h"
#include "core/core_timing.h"
#include "core/hle/kernel/svc.h"
#include "core/memory.h"

namespace Core {

class DynarmicCallbacks32 : public Dynarmic::A32::UserCallbacks {
public:
    explicit DynarmicCallbacks32(ARM_Dynarmic_32& parent) : parent(parent) {}

    u8 MemoryRead8(u32 vaddr) override {
        return parent.system.Memory().Read8(vaddr);
    }
    u16 MemoryRead16(u32 vaddr) override {
        return parent.system.Memory().Read16(vaddr);
    }
    u32 MemoryRead32(u32 vaddr) override {
        return parent.system.Memory().Read32(vaddr);
    }
    u64 MemoryRead64(u32 vaddr) override {
        return parent.system.Memory().Read64(vaddr);
    }

    void MemoryWrite8(u32 vaddr, u8 value) override {
        parent.system.Memory().Write8(vaddr, value);
    }
    void MemoryWrite16(u32 vaddr, u16 value) override {
        parent.system.Memory().Write16(vaddr, value);
    }
    void MemoryWrite32(u32 vaddr, u32 value) override {
        parent.system.Memory().Write32(vaddr, value);
    }
    void MemoryWrite64(u32 vaddr, u64 value) override {
        parent.system.Memory().Write64(vaddr, value);
    }

    void InterpreterFallback(u32 pc, std::size_t num_instructions) override {
        UNIMPLEMENTED_MSG("This should never happen, pc = {:08X}, code = {:08X}", pc,
                          MemoryReadCode(pc));
    }

    void ExceptionRaised(u32 pc, Dynarmic::A32::Exception exception) override {
        switch (exception) {
        case Dynarmic::A32::Exception::UndefinedInstruction:
        case Dynarmic::A32::Exception::UnpredictableInstruction:
            break;
        case Dynarmic::A32::Exception::Breakpoint:
            break;
        }
        LOG_CRITICAL(Core_ARM, "ExceptionRaised(exception = {}, pc = {:08X}, code = {:08X})",
                     static_cast<std::size_t>(exception), pc, MemoryReadCode(pc));
        UNIMPLEMENTED();
    }

    void CallSVC(u32 swi) override {
        Kernel::Svc::Call(parent.system, swi);
    }

    void AddTicks(u64 ticks) override {
        // Divide the number of ticks by the amount of CPU cores. TODO(Subv): This yields only a
        // rough approximation of the amount of executed ticks in the system, it may be thrown off
        // if not all cores are doing a similar amount of work. Instead of doing this, we should
        // device a way so that timing is consistent across all cores without increasing the ticks 4
        // times.
        u64 amortized_ticks = (ticks - num_interpreted_instructions) / Core::NUM_CPU_CORES;
        // Always execute at least one tick.
        amortized_ticks = std::max<u64>(amortized_ticks, 1);

        parent.system.CoreTiming().AddTicks(amortized_ticks);
        num_interpreted_instructions = 0;
    }
    u64 GetTicksRemaining() override {
        return std::max(parent.system.CoreTiming().GetDowncount(), {});
    }

    ARM_Dynarmic_32& parent;
    std::size_t num_interpreted_instructions{};
};

std::shared_ptr<Dynarmic::A32::Jit> ARM_Dynarmic_32::MakeJit(Common::PageTable& page_table,
                                                             std::size_t address_space_bits) const {
    Dynarmic::A32::UserConfig config;
    config.callbacks = cb.get();
    // TODO(bunnei): Implement page table for 32-bit
    // config.page_table = &page_table.pointers;
    config.coprocessors[15] = cp15;
    config.define_unpredictable_behaviour = true;
    return std::make_unique<Dynarmic::A32::Jit>(config);
}

MICROPROFILE_DEFINE(ARM_Jit_Dynarmic_32, "ARM JIT", "Dynarmic", MP_RGB(255, 64, 64));

void ARM_Dynarmic_32::Run() {
    MICROPROFILE_SCOPE(ARM_Jit_Dynarmic_32);
    jit->Run();
}

void ARM_Dynarmic_32::Step() {
    jit->Step();
}

ARM_Dynarmic_32::ARM_Dynarmic_32(System& system, ExclusiveMonitor& exclusive_monitor,
                                 std::size_t core_index)
    : ARM_Interface{system}, cb(std::make_unique<DynarmicCallbacks32>(*this)),
      cp15(std::make_shared<DynarmicCP15>(*this)), core_index{core_index},
      exclusive_monitor{dynamic_cast<DynarmicExclusiveMonitor&>(exclusive_monitor)} {}

ARM_Dynarmic_32::~ARM_Dynarmic_32() = default;

void ARM_Dynarmic_32::SetPC(u64 pc) {
    jit->Regs()[15] = static_cast<u32>(pc);
}

u64 ARM_Dynarmic_32::GetPC() const {
    return jit->Regs()[15];
}

u64 ARM_Dynarmic_32::GetReg(int index) const {
    return jit->Regs()[index];
}

void ARM_Dynarmic_32::SetReg(int index, u64 value) {
    jit->Regs()[index] = static_cast<u32>(value);
}

u128 ARM_Dynarmic_32::GetVectorReg(int index) const {
    return {};
}

void ARM_Dynarmic_32::SetVectorReg(int index, u128 value) {}

u32 ARM_Dynarmic_32::GetPSTATE() const {
    return jit->Cpsr();
}

void ARM_Dynarmic_32::SetPSTATE(u32 cpsr) {
    jit->SetCpsr(cpsr);
}

u64 ARM_Dynarmic_32::GetTlsAddress() const {
    return cp15->uro;
}

void ARM_Dynarmic_32::SetTlsAddress(VAddr address) {
    cp15->uro = static_cast<u32>(address);
}

u64 ARM_Dynarmic_32::GetTPIDR_EL0() const {
    return cp15->uprw;
}

void ARM_Dynarmic_32::SetTPIDR_EL0(u64 value) {
    cp15->uprw = static_cast<u32>(value);
}

void ARM_Dynarmic_32::SaveContext(ThreadContext32& ctx) {
    Dynarmic::A32::Context context;
    jit->SaveContext(context);
    ctx.cpu_registers = context.Regs();
    ctx.cpsr = context.Cpsr();
}

void ARM_Dynarmic_32::LoadContext(const ThreadContext32& ctx) {
    Dynarmic::A32::Context context;
    context.Regs() = ctx.cpu_registers;
    context.SetCpsr(ctx.cpsr);
    jit->LoadContext(context);
}

void ARM_Dynarmic_32::PrepareReschedule() {
    jit->HaltExecution();
}

void ARM_Dynarmic_32::ClearInstructionCache() {
    jit->ClearCache();
}

void ARM_Dynarmic_32::ClearExclusiveState() {}

void ARM_Dynarmic_32::PageTableChanged(Common::PageTable& page_table,
                                       std::size_t new_address_space_size_in_bits) {
    auto key = std::make_pair(&page_table, new_address_space_size_in_bits);
    auto iter = jit_cache.find(key);
    if (iter != jit_cache.end()) {
        jit = iter->second;
        return;
    }
    jit = MakeJit(page_table, new_address_space_size_in_bits);
    jit_cache.emplace(key, jit);
}

} // namespace Core
