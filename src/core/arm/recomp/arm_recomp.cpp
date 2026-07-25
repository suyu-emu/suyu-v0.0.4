// SPDX-FileCopyrightText: Copyright 2026 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <atomic>
#include <cstring>

#include "common/logging/log.h"
#include "core/arm/recomp/arm_recomp.h"
#include "core/core.h"
#include "core/hle/kernel/k_thread.h"

namespace Core {

namespace {
// Mirrors the prefix of the GuestContext the recompiler emits. Only the fields
// the emulator needs to observe or mutate are modelled; the generated struct
// carries additional members after these (heap bookkeeping, save-data handles)
// which the recompiled code manages itself and we never touch.
struct GuestContextView {
    u64 x[32];
    u64 pc;
    u8 n, z, c, v;
    u8* mem;
    u64 mem_size;
    u64 mem_base_vaddr;
    int halted;
    u64 pending_svc;
};

// The generated code signals an SVC by parking with this set. Kept in sync
// with the emitted recomp_svc contract in core/recompiler/arm64_to_c.h.
constexpr u64 kNoPendingSvc = ~0ULL;
} // namespace

struct ArmRecomp::Impl {
    Impl(System& system_, RecompLookupFn lookup_) : system{system_}, lookup{lookup_} {
        std::memset(&ctx, 0, sizeof(ctx));
        ctx.pending_svc = kNoPendingSvc;
    }

    System& system;
    RecompLookupFn lookup{};
    GuestContextView ctx{};
    u64 tpidrro_el0{};
    std::atomic<bool> interrupted{false};
};

ArmRecomp::ArmRecomp(System& system, bool uses_wall_clock, RecompLookupFn lookup)
    : ArmInterface{uses_wall_clock}, impl{std::make_unique<Impl>(system, lookup)} {}

ArmRecomp::~ArmRecomp() = default;

HaltReason ArmRecomp::RunThread(Kernel::KThread* thread) {
    if (!impl->lookup) {
        LOG_ERROR(Core_ARM, "No recompiled code registered; cannot run thread");
        return HaltReason::BreakLoop;
    }

    impl->interrupted.store(false, std::memory_order_relaxed);
    impl->ctx.halted = 0;

    while (!impl->ctx.halted) {
        if (impl->interrupted.load(std::memory_order_relaxed)) {
            return HaltReason::BreakLoop;
        }

        // An SVC parked us last time round; the kernel has now serviced it and
        // resumed, so clear it before continuing.
        if (impl->ctx.pending_svc != kNoPendingSvc) {
            impl->ctx.pending_svc = kNoPendingSvc;
        }

        const RecompBlockFn block = impl->lookup(impl->ctx.pc);
        if (!block) {
            // No recompiled block covers this address. This is a genuine gap
            // (indirect branch into code the static pass never reached), not
            // something to paper over - report it rather than silently
            // executing the wrong thing.
            LOG_ERROR(Core_ARM, "No recompiled block at PC {:#x}", impl->ctx.pc);
            return HaltReason::PrefetchAbort;
        }

        block(&impl->ctx);

        if (impl->ctx.pending_svc != kNoPendingSvc) {
            return HaltReason::SupervisorCall;
        }
    }

    return HaltReason::BreakLoop;
}

HaltReason ArmRecomp::StepThread(Kernel::KThread* thread) {
    // Block granularity is the finest this backend can step: recompiled blocks
    // are straight-line C with no per-instruction re-entry point.
    if (!impl->lookup) {
        return HaltReason::BreakLoop;
    }
    const RecompBlockFn block = impl->lookup(impl->ctx.pc);
    if (!block) {
        return HaltReason::PrefetchAbort;
    }
    block(&impl->ctx);
    if (impl->ctx.pending_svc != kNoPendingSvc) {
        return HaltReason::SupervisorCall;
    }
    return HaltReason::StepThread;
}

void ArmRecomp::ClearInstructionCache() {
    // Statically recompiled code is fixed at build time; there is no
    // translation cache to invalidate. Self-modifying guest code is
    // consequently unsupported by this backend by construction.
}

void ArmRecomp::InvalidateCacheRange(u64 addr, std::size_t size) {
    // See ClearInstructionCache.
}

void ArmRecomp::GetContext(Kernel::Svc::ThreadContext& ctx) const {
    std::memset(&ctx, 0, sizeof(ctx));
    for (size_t i = 0; i < 29; ++i) {
        ctx.r[i] = impl->ctx.x[i];
    }
    ctx.fp = impl->ctx.x[29];
    ctx.lr = impl->ctx.x[30];
    ctx.sp = impl->ctx.x[31];
    ctx.pc = impl->ctx.pc;
    ctx.pstate = (static_cast<u32>(impl->ctx.n) << 31) |
                 (static_cast<u32>(impl->ctx.z) << 30) |
                 (static_cast<u32>(impl->ctx.c) << 29) |
                 (static_cast<u32>(impl->ctx.v) << 28);
    ctx.tpidr = impl->tpidrro_el0;
}

void ArmRecomp::SetContext(const Kernel::Svc::ThreadContext& ctx) {
    for (size_t i = 0; i < 29; ++i) {
        impl->ctx.x[i] = ctx.r[i];
    }
    impl->ctx.x[29] = ctx.fp;
    impl->ctx.x[30] = ctx.lr;
    impl->ctx.x[31] = ctx.sp;
    impl->ctx.pc = ctx.pc;
    impl->ctx.n = (ctx.pstate >> 31) & 1;
    impl->ctx.z = (ctx.pstate >> 30) & 1;
    impl->ctx.c = (ctx.pstate >> 29) & 1;
    impl->ctx.v = (ctx.pstate >> 28) & 1;
    impl->tpidrro_el0 = ctx.tpidr;
}

void ArmRecomp::SetTpidrroEl0(u64 value) {
    impl->tpidrro_el0 = value;
}

void ArmRecomp::GetSvcArguments(std::span<uint64_t, 8> args) const {
    for (size_t i = 0; i < 8; ++i) {
        args[i] = impl->ctx.x[i];
    }
}

void ArmRecomp::SetSvcArguments(std::span<const uint64_t, 8> args) {
    for (size_t i = 0; i < 8; ++i) {
        impl->ctx.x[i] = args[i];
    }
}

u32 ArmRecomp::GetSvcNumber() const {
    return static_cast<u32>(impl->ctx.pending_svc);
}

void ArmRecomp::SignalInterrupt(Kernel::KThread* thread) {
    impl->interrupted.store(true, std::memory_order_relaxed);
}

const Kernel::DebugWatchpoint* ArmRecomp::HaltedWatchpoint() const {
    return nullptr;
}

void ArmRecomp::RewindBreakpointInstruction() {
    // No breakpoint patching in statically recompiled code.
}

} // namespace Core
