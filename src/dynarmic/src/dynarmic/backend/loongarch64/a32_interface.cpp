// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <array>
#include <memory>
#include <string>
#include <utility>

#include "common/assert.h"
#include "common/common_types.h"
#include "dynarmic/interface/A32/a32.h"

namespace Dynarmic::A32 {

struct Jit::Impl final {
    explicit Impl(UserConfig conf_) : conf(std::move(conf_)) {}

    HaltReason Run() {
        UNIMPLEMENTED();
        return halt_reason;
    }

    HaltReason Step() {
        UNIMPLEMENTED();
        return halt_reason | HaltReason::Step;
    }

    void ClearCache() {
        HaltExecution(HaltReason::CacheInvalidation);
    }

    void InvalidateCacheRange(u32, std::size_t) {
        HaltExecution(HaltReason::CacheInvalidation);
    }

    void Reset() {
        regs = {};
        ext_regs = {};
        cpsr = 0;
        fpscr = 0;
        halt_reason = {};
    }

    void HaltExecution(HaltReason hr) {
        halt_reason |= hr;
    }

    void ClearHalt(HaltReason hr) {
        halt_reason &= ~hr;
    }

    std::array<u32, 16>& Regs() {
        return regs;
    }

    const std::array<u32, 16>& Regs() const {
        return regs;
    }

    std::array<u32, 64>& ExtRegs() {
        return ext_regs;
    }

    const std::array<u32, 64>& ExtRegs() const {
        return ext_regs;
    }

    u32 Cpsr() const {
        return cpsr;
    }

    void SetCpsr(u32 value) {
        cpsr = value;
    }

    u32 Fpscr() const {
        return fpscr;
    }

    void SetFpscr(u32 value) {
        fpscr = value;
    }

    void ClearExclusiveState() {}

    std::string Disassemble() const {
        return {};
    }

    UserConfig conf;
    std::array<u32, 16> regs{};
    std::array<u32, 64> ext_regs{};
    u32 cpsr = 0;
    u32 fpscr = 0;
    HaltReason halt_reason{};
};

Jit::Jit(UserConfig conf) : impl(std::make_unique<Impl>(std::move(conf))) {}

Jit::~Jit() = default;

HaltReason Jit::Run() {
    return impl->Run();
}

HaltReason Jit::Step() {
    return impl->Step();
}

void Jit::ClearCache() {
    impl->ClearCache();
}

void Jit::InvalidateCacheRange(u32 start_address, std::size_t length) {
    impl->InvalidateCacheRange(start_address, length);
}

void Jit::Reset() {
    impl->Reset();
}

void Jit::HaltExecution(HaltReason hr) {
    impl->HaltExecution(hr);
}

void Jit::ClearHalt(HaltReason hr) {
    impl->ClearHalt(hr);
}

std::array<u32, 16>& Jit::Regs() {
    return impl->Regs();
}

const std::array<u32, 16>& Jit::Regs() const {
    return impl->Regs();
}

std::array<u32, 64>& Jit::ExtRegs() {
    return impl->ExtRegs();
}

const std::array<u32, 64>& Jit::ExtRegs() const {
    return impl->ExtRegs();
}

u32 Jit::Cpsr() const {
    return impl->Cpsr();
}

void Jit::SetCpsr(u32 value) {
    impl->SetCpsr(value);
}

u32 Jit::Fpscr() const {
    return impl->Fpscr();
}

void Jit::SetFpscr(u32 value) {
    impl->SetFpscr(value);
}

void Jit::ClearExclusiveState() {
    impl->ClearExclusiveState();
}

std::string Jit::Disassemble() const {
    return impl->Disassemble();
}

}  // namespace Dynarmic::A32
