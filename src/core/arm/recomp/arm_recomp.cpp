// SPDX-FileCopyrightText: Copyright 2026 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <atomic>
#include <cstring>
#include <string>

#include "common/logging/log.h"
#include "core/arm/recomp/arm_recomp.h"
#include "core/core.h"
#include "core/hle/kernel/k_thread.h"
#include "core/arm/debug.h"
#include "core/memory.h"

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
    // The SIMD/FP register file and thread pointer sit immediately after
    // pending_svc in the emitted struct. They have to be modelled here rather
    // than left off the end: the recompiler emits SIMD code, so a context
    // switch that did not carry these would silently lose every floating-point
    // and vector register the guest had live.
    u64 vreg[32][2];
    u64 tpidr_el0;
    const void* host_mem;
};

// Matches RecompHostMem in the generated runtime. The recompiled code calls
// through this for every guest access, so that it reads and writes the
// emulator's address space rather than the flat buffer the standalone runtime
// would otherwise own - without it the recompiled code and the HLE kernel
// would be looking at two different memories.
struct RecompHostMem {
    void* user;
    u64 (*load)(void* user, u64 va, u32 size);
    void (*store)(void* user, u64 va, u32 size, u64 value);
};

// Nothing links these two builds together, so the shared layout is pinned on
// both sides: the generated runtime asserts the same four offsets against its
// own GuestContext. If a field is ever inserted rather than appended, one of
// the two fails to compile instead of the emulator silently reading the wrong
// registers.
static_assert(offsetof(GuestContextView, pc) == 256);
static_assert(offsetof(GuestContextView, pending_svc) == 304);
static_assert(offsetof(GuestContextView, vreg) == 312);
static_assert(offsetof(GuestContextView, tpidr_el0) == 824);
static_assert(offsetof(GuestContextView, host_mem) == 832);

// The generated code signals an SVC by parking with this set. Kept in sync
// with the emitted recomp_svc contract in core/recompiler/arm64_to_c.h.
constexpr u64 kNoPendingSvc = ~0ULL;
} // namespace

namespace {
std::atomic<RecompLookupFn> g_recomp_lookup{nullptr};
std::atomic<RecompBaseFn> g_recomp_base_setter{nullptr};
} // namespace

void SetRecompLookup(RecompLookupFn lookup) {
    g_recomp_lookup.store(lookup, std::memory_order_release);
}

void SetRecompBaseSetter(RecompBaseFn setter) {
    g_recomp_base_setter.store(setter, std::memory_order_release);
}

RecompLookupFn GetRecompLookup() {
    return g_recomp_lookup.load(std::memory_order_acquire);
}

struct ArmRecomp::Impl {
    Impl(System& system_, RecompLookupFn lookup_) : system{system_}, lookup{lookup_} {
        std::memset(&ctx, 0, sizeof(ctx));
        ctx.pending_svc = kNoPendingSvc;
        // Point the recompiled code at the emulator's address space.
        bridge.user = this;
        bridge.load = &Impl::HostLoad;
        bridge.store = &Impl::HostStore;
        ctx.host_mem = &bridge;
    }

    static u64 HostLoad(void* user, u64 va, u32 size) {
        auto& memory = static_cast<Impl*>(user)->system.ApplicationMemory();
        switch (size) {
        case 1: return memory.Read8(va);
        case 2: return memory.Read16(va);
        case 4: return memory.Read32(va);
        default: return memory.Read64(va);
        }
    }

    static void HostStore(void* user, u64 va, u32 size, u64 value) {
        auto& memory = static_cast<Impl*>(user)->system.ApplicationMemory();
        switch (size) {
        case 1: memory.Write8(va, static_cast<u8>(value)); break;
        case 2: memory.Write16(va, static_cast<u16>(value)); break;
        case 4: memory.Write32(va, static_cast<u32>(value)); break;
        default: memory.Write64(va, value); break;
        }
    }

    /// Base address of the module containing `pc`, so an address can be turned
    /// into the module-relative offset a recompiled image is keyed by. The
    /// module list is fixed once the process is running, so it is read once.
    u64 ModuleBaseFor(Kernel::KThread* thread, u64 pc) {
        if (!modules_read) {
            modules_read = true;
            if (auto* process = thread->GetOwnerProcess()) {
                modules = FindModules(process);
                // Now that the loader has placed everything, tell each image
                // where its own module went.
                if (const auto setter = g_recomp_base_setter.load(std::memory_order_acquire)) {
                    // modules is keyed by base, so iteration is load order.
                    size_t index = 0;
                    for (const auto& [module_base, name] : modules) {
                        setter(index++, name.c_str(), module_base);
                    }
                }
            }
        }
        u64 base = 0;
        for (const auto& [module_base, name] : modules) {
            if (pc >= module_base && module_base >= base) {
                base = module_base;
            }
        }
        return base;
    }

    // Pre-apply R_AARCH64_RELATIVE relocations for a module by walking its
    // dynamic section (found via MOD0). Under dynarmic, rtld runs its own
    // self-relocation loop correctly; under ArmRecomp the recompiled loop exits
    // early, leaving most relocations un-applied and corrupting data reads.
    void ApplyRelaRelocations(u64 mod_base) {
        auto& mem = system.ApplicationMemory();
        // MOD0 magic "MOD0" = 0x30444F4D. It sits at the start of rodata
        // (typically mod+0x2000 for rtld), but the actual location is pointed
        // to by a 4-byte offset at mod+4 (per NSO ABI). Try both locations.
        // Scan the first few KB for the MOD0 signature.
        u64 mod0_va = 0;
        for (u64 off = 0; off < 0x4000; off += 4) {
            if (mem.Read32(mod_base + off) == 0x30444F4Du) {
                mod0_va = mod_base + off;
                break;
            }
        }
        if (!mod0_va) return;

        // MOD0 layout: magic(4), dyn_offset(4), bss_start(4), bss_end(4)
        // dyn_offset is relative to the MOD0 header itself.
        const u32 dyn_rel_off = mem.Read32(mod0_va + 4);
        const u64 dyn_va = mod0_va + dyn_rel_off;

        // Parse the dynamic section (64-bit tag-value pairs).
        constexpr u32 DT_NULL = 0, DT_RELA = 7, DT_RELASZ = 8, DT_RELAENT = 9;
        u64 rela_va = 0, rela_sz = 0, rela_ent = 24;
        for (u64 p = dyn_va; ; p += 16) {
            const u64 tag = mem.Read64(p);
            const u64 val = mem.Read64(p + 8);
            if (tag == DT_NULL) break;
            if (tag == DT_RELA)   rela_va  = mod_base + val;
            if (tag == DT_RELASZ) rela_sz  = val;
            if (tag == DT_RELAENT) rela_ent = val;
            if (p - dyn_va > 0x1000) break; // safety
        }
        if (!rela_va || !rela_sz) return;

        // Apply each R_AARCH64_RELATIVE entry (type 0x403).
        constexpr u32 R_AARCH64_RELATIVE = 0x403;
        u32 applied = 0;
        for (u64 p = rela_va; p < rela_va + rela_sz; p += rela_ent) {
            const u64 r_offset = mem.Read64(p);
            const u64 r_info   = mem.Read64(p + 8);
            const u64 r_addend = mem.Read64(p + 16);
            if ((r_info & 0xFFFFFFFF) == R_AARCH64_RELATIVE) {
                mem.Write64(mod_base + r_offset, mod_base + r_addend);
                ++applied;
            }
        }
        LOG_INFO(Core_ARM, "recomp: pre-applied {} RELA relocations for module base={:#x}", applied, mod_base);
    }

    System& system;
    RecompLookupFn lookup{};
    GuestContextView ctx{};
    RecompHostMem bridge{};
    u64 tpidrro_el0{};
    std::atomic<bool> interrupted{false};
    Loader::AppLoader::Modules modules{};
    bool modules_read{false};
    bool rela_applied{false};
    static constexpr size_t kTrail = 32;
    u64 trail[kTrail]{};
    size_t trail_pos{0};
};

ArmRecomp::ArmRecomp(System& system, bool uses_wall_clock, RecompLookupFn lookup)
    : ArmInterface{uses_wall_clock}, impl{std::make_unique<Impl>(system, lookup)} {}

ArmRecomp::~ArmRecomp() = default;

HaltReason ArmRecomp::RunThread(Kernel::KThread* thread) {
    // Logged once so it is obvious from a log whether the backend was ever
    // entered at all. A run with no errors is otherwise indistinguishable from
    // a run where the guest thread was never scheduled onto it.
    static bool announced = false;
    if (!announced) {
        announced = true;
        LOG_INFO(Core_ARM, "ArmRecomp::RunThread entered, pc={:#x}", impl->ctx.pc);
    }
    if (!impl->lookup) {
        LOG_ERROR(Core_ARM, "No recompiled code registered; cannot run thread");
        return HaltReason::BreakLoop;
    }

    // Registering every loaded image's base with the host dispatcher is a
    // side effect of this call, not something its return value is used for
    // here - the dispatcher needs it done once before the first lookup, or
    // every image's base stays 0 and every lookup misses.
    static bool bases_registered = false;
    if (!bases_registered) {
        bases_registered = true;
        impl->ModuleBaseFor(thread, impl->ctx.pc);
    }

    if (!impl->rela_applied) {
        impl->rela_applied = true;
        // Snapshot 0x273c BEFORE our RELA pass to pinpoint corruption source.
        auto dump273c = [&](const char* tag) {
            if (impl->modules.empty()) return;
            const u64 mbase = impl->modules.begin()->first;
            std::string dump;
            for (u64 i = 0; i < 0x60; i += 4)
                dump += fmt::format("{:08x} ", Impl::HostLoad(impl.get(), mbase + 0x273c + i, 4));
            LOG_INFO(Core_ARM, "recomp {} mod+0x273c (base {:#x}): {}", tag, mbase, dump);
        };
        dump273c("BEFORE-RELA");
        for (const auto& [module_base, name] : impl->modules)
            impl->ApplyRelaRelocations(module_base);
        dump273c("AFTER-RELA");
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

        // A recompiled image is keyed by each block's offset within its own
        // module, because that is all the static pass can know: an NSO's
        // segment header carries the offset inside the module, not the address
        // the loader will map it to, and that address changes per run anyway.
        // The host-side dispatcher (suyu's chained lookup) owns picking which
        // image the PC belongs to and reducing to that image's offset before
        // calling into it - a second offset-based retry here used to guess
        // which image based only on pc-base, but two images can both define a
        // block at the same offset (every module has one at offset 0), so a
        // guess made without knowing which image owns the address silently
        // ran the wrong module's code with no error. Ask with the absolute PC
        // and let the dispatcher own the reduction.
        // Rolling trail of the last few PCs. A wild indirect branch reports
        // only the address it landed on, which says nothing about which block
        // computed it; without the predecessors there is no way to tell a bad
        // GOT read from a bad emitted branch.
        impl->trail[impl->trail_pos++ & (Impl::kTrail - 1)] = impl->ctx.pc;

        // Log pre-dispatch state for the jump-table block to diagnose the GOT bug.
        const u64 base = impl->modules.empty() ? 0 : impl->modules.begin()->first;
        if (impl->ctx.pc - base == 0x34c && base != 0) {
            LOG_ERROR(Core_ARM, "recomp jt dispatch: base={:#x} x0={:#x} x18={:#x} mem@x18={:#x} mem@x18+x0*4={:#x}",
                      base, impl->ctx.x[0], impl->ctx.x[18],
                      Impl::HostLoad(impl.get(), impl->ctx.x[18], 4),
                      Impl::HostLoad(impl.get(), impl->ctx.x[18] + impl->ctx.x[0]*4, 4));
        }

        RecompBlockFn block = impl->lookup(impl->ctx.pc);
        if (!block) {
            std::string trail;
            const size_t count = std::min<size_t>(impl->trail_pos, Impl::kTrail);
            for (size_t i = 0; i < count; ++i) {
                const u64 p = impl->trail[(impl->trail_pos - count + i) & (Impl::kTrail - 1)];
                trail += fmt::format("{:#x} ", p);
            }
            LOG_ERROR(Core_ARM, "recomp PC trail (oldest first): {}", trail);
            LOG_ERROR(Core_ARM, "recomp regs x16={:#x} x17={:#x} x30={:#x} sp={:#x}",
                      impl->ctx.x[16], impl->ctx.x[17], impl->ctx.x[30], impl->ctx.x[31]);
            LOG_ERROR(Core_ARM, "recomp regs x0={:#x} x15={:#x} x18={:#x} x19={:#x}",
                      impl->ctx.x[0], impl->ctx.x[15], impl->ctx.x[18], impl->ctx.x[19]);
            {
                const u64 mbase = impl->modules.empty() ? 0 : impl->modules.begin()->first;
                for (u64 seg : {0x0ULL, 0x2000ULL, 0x3000ULL}) {
                    std::string dump;
                    for (u64 i = 0; i < 0x40; i += 4) {
                        dump += fmt::format("{:08x} ", Impl::HostLoad(impl.get(), mbase + seg + i, 4));
                    }
                    LOG_ERROR(Core_ARM, "recomp mem mod+{:#x} (base {:#x}): {}", seg, mbase, dump);
                }
            }
        }
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
    // u128 here is a pair of 64-bit halves, matching how the generated
    // context stores each vector register.
    for (size_t i = 0; i < 32; ++i) {
        ctx.v[i][0] = impl->ctx.vreg[i][0];
        ctx.v[i][1] = impl->ctx.vreg[i][1];
    }
    ctx.tpidr = impl->ctx.tpidr_el0;
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
    for (size_t i = 0; i < 32; ++i) {
        impl->ctx.vreg[i][0] = ctx.v[i][0];
        impl->ctx.vreg[i][1] = ctx.v[i][1];
    }
    impl->ctx.tpidr_el0 = ctx.tpidr;
    impl->tpidrro_el0 = ctx.tpidr;
}

void ArmRecomp::SetTpidrroEl0(u64 value) {
    impl->tpidrro_el0 = value;
    // The emitted MRS/MSR handlers read the thread pointer out of the guest
    // context, so setting only the local copy left every guest read of
    // TPIDR_EL0 returning zero - which breaks thread-local storage.
    impl->ctx.tpidr_el0 = value;
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
