/* This file is part of the dynarmic project.
 * Copyright (c) 2024 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#include <memory>
#include <mutex>

#include <boost/icl/interval_set.hpp>
#include <mcl/assert.hpp>
#include <mcl/scope_exit.hpp>
#include <mcl/stdint.hpp>

#include "dynarmic/backend/riscv64/a32_address_space.h"
#include "dynarmic/backend/riscv64/a32_core.h"
#include "dynarmic/backend/riscv64/a32_jitstate.h"
#include "dynarmic/common/atomic.h"
#include "dynarmic/interface/A32/a32.h"

namespace Dynarmic::A32 {

using namespace Backend::RV64;

struct Jit::Impl final {
    Impl(Jit* jit_interface, A32::UserConfig conf)
            : jit_interface(jit_interface)
            , conf(conf)
            , current_address_space(conf)
            , core(conf) {}

    HaltReason Run() {
        ASSERT(!jit_interface->is_executing);
        jit_interface->is_executing = true;
        SCOPE_EXIT {
            jit_interface->is_executing = false;
        };

        HaltReason hr = core.Run(current_address_space, current_state, &halt_reason);

        RequestCacheInvalidation();

        return hr;
    }

    HaltReason Step() {
        ASSERT(!jit_interface->is_executing);
        jit_interface->is_executing = true;
        SCOPE_EXIT {
            jit_interface->is_executing = false;
        };

        UNIMPLEMENTED();

        RequestCacheInvalidation();

        return HaltReason{};
    }

    void ClearCache() {
        std::unique_lock lock{invalidation_mutex};
        invalidate_entire_cache = true;
        HaltExecution(HaltReason::CacheInvalidation);
    }

    void InvalidateCacheRange(u32 start_address, size_t length) {
        std::unique_lock lock{invalidation_mutex};
        invalid_cache_ranges.add(boost::icl::discrete_interval<u32>::closed(start_address, static_cast<u32>(start_address + length - 1)));
        HaltExecution(HaltReason::CacheInvalidation);
    }

    void Reset() {
        current_state = {};
    }

    void HaltExecution(HaltReason hr) {
        Atomic::Or(&halt_reason, ~static_cast<u32>(hr));
    }

    void ClearHalt(HaltReason hr) {
        Atomic::And(&halt_reason, ~static_cast<u32>(hr));
    }

    std::array<u32, 16>& Regs() {
        return current_state.regs;
    }

    const std::array<u32, 16>& Regs() const {
        return current_state.regs;
    }

    std::array<u32, 64>& ExtRegs() {
        return current_state.ext_regs;
    }

    const std::array<u32, 64>& ExtRegs() const {
        return current_state.ext_regs;
    }

    u32 Cpsr() const {
        return current_state.Cpsr();
    }

    void SetCpsr(u32 value) {
        current_state.SetCpsr(value);
    }

    u32 Fpscr() const {
        return current_state.Fpscr();
    }

    void SetFpscr(u32 value) {
        current_state.SetFpscr(value);
    }

    void ClearExclusiveState() {
        current_state.exclusive_state = false;
    }

    void DumpDisassembly() const {
        UNIMPLEMENTED();
    }

private:
    void RequestCacheInvalidation() {
        // ASSERT_FALSE("Unimplemented");

        invalidate_entire_cache = false;
        invalid_cache_ranges.clear();
    }

    Jit* jit_interface;
    A32::UserConfig conf;
    A32JitState current_state{};
    A32AddressSpace current_address_space;
    A32Core core;

    volatile u32 halt_reason = 0;

    std::mutex invalidation_mutex;
    boost::icl::interval_set<u32> invalid_cache_ranges;
    bool invalidate_entire_cache = false;
};

Jit::Jit(UserConfig conf)
        : impl(std::make_unique<Impl>(this, conf)) {}

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

void Jit::DumpDisassembly() const {
    impl->DumpDisassembly();
}

}  // namespace Dynarmic::A32
