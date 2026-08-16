// SPDX-FileCopyrightText: Copyright 2026 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/logging/log.h"
#include "core/arm/recomp/arm_recomp.h"
#include "core/core.h"
#include "core/hle/kernel/k_thread.h"
#include "core/arm/debug.h"
#include "core/arm/dynarmic/arm_dynarmic_64.h"
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
    // TPIDRRO_EL0 - the kernel-published thread-local region, whose first
    // 0x100 bytes are the IPC message buffer KServerSession reads a
    // SendSyncRequest out of. Kept apart from tpidr_el0 (the guest's own
    // thread pointer) because the two have different owners and different
    // lifetimes; see the note on GuestContext in core/recompiler/arm64_to_c.h.
    u64 tpidrro_el0;
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
static_assert(offsetof(GuestContextView, tpidrro_el0) == 840);

// The generated code signals an SVC by parking with this set. Kept in sync
// with the emitted recomp_svc contract in core/recompiler/arm64_to_c.h.
constexpr u64 kNoPendingSvc = ~0ULL;

// Mirrors RECOMP_HALT_UNHANDLED in the generated runtime: a block that halts
// with this parked its PC on an instruction the decoder cannot translate and
// is asking for that address to be executed by the interpreter fallback.
constexpr int kHaltUnhandled = 2;

// An unresolved GOT/JUMP_SLOT relocation used to be left untouched, which
// means a call through it branches to whatever the raw NSO file already had
// sitting in that GOT slot - typically a small placeholder/addend value the
// static linker left for a symbol it expected the *dynamic* linker to fill
// in later (lazy-binding stub offset, or just zero-adjacent garbage), not a
// real address. The guest's BLR then lands on that small value directly -
// e.g. 0xe7ff0 - which is unmapped, and the JIT fallback that "No recompiled
// block" hands off to can't execute there either, so the whole thread dies.
// Every unresolved slot is patched to this fixed, recognizable sentinel
// instead: the dispatch loop below special-cases it as an immediate "return
// to caller" (PC = LR) rather than a real guest address, so a genuinely
// call-but-never-actually-invoked unresolved import (the common case - most
// entries in a large import table exist for code paths a given boot never
// takes) fails soft instead of crashing the thread outright.
constexpr u64 kUnresolvedImportTrap = 0xFFFF'FFFF'0000'0000ULL;
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

    struct DynInfo {
        u64 mod_base = 0;
        u64 rela_va = 0, rela_sz = 0, rela_ent = 24, rela_sz_va = 0;
        u64 jmprel_va = 0, jmprel_sz = 0, jmprel_ent = 24, jmprel_sz_va = 0;
        u64 symtab_va = 0, strtab_va = 0;
        // Address of a harmless "return 0" stub inside this module's own text,
        // used as the target for every import that could not be resolved.
        u64 trap_va = 0;
    };

    // Find an existing `mov x0, #0; ret` (or failing that, a bare `ret`) in a
    // module's text and hand back its address.
    //
    // Unresolved imports have to point somewhere, and the address has to be one
    // BOTH execution engines can survive: the recompiled dispatcher can
    // special-case a magic sentinel, but the dynarmic fallback cannot - it just
    // tries to translate the address and dies with "cannot execute instruction
    // at unmapped address". Reusing a real instruction pair the module already
    // contains sidesteps that entirely: it is mapped, executable, and returns to
    // the caller under any engine, with no per-engine handling and nothing
    // written into guest memory. Most entries in a large import table exist for
    // code paths a given boot never takes, so failing soft here is what keeps a
    // partially-resolvable module bootable at all.
    u64 FindReturnStub(u64 mod_base) {
        auto& mem = system.ApplicationMemory();
        constexpr u32 kMovX0Zero = 0xD2800000, kRet = 0xD65F03C0;
        // Text sits at the front of every NSO; a module without a matching pair
        // in its first megabyte does not have one worth scanning further for.
        constexpr u64 kScanLimit = 0x100000;
        u64 bare_ret = 0;
        for (u64 off = 0; off < kScanLimit; off += 4) {
            const u32 insn = mem.Read32(mod_base + off);
            if (insn == kRet) {
                if (!bare_ret) bare_ret = mod_base + off;
            } else if (insn == kMovX0Zero && mem.Read32(mod_base + off + 4) == kRet) {
                return mod_base + off;
            }
        }
        return bare_ret;
    }

    // Locate a module's MOD0 header and parse its .dynamic section. Returns
    // false if this module has no MOD0 (nothing to relocate).
    bool ParseDynamic(u64 mod_base, DynInfo& out) {
        auto& mem = system.ApplicationMemory();
        // MOD0 magic "MOD0" = 0x30444F4D. It sits at the start of rodata
        // (typically mod+0x2000 for rtld), but the actual location is pointed
        // to by a 4-byte offset at mod+4 (per NSO ABI). Scan the first few KB.
        u64 mod0_va = 0;
        for (u64 off = 0; off < 0x4000; off += 4) {
            if (mem.Read32(mod_base + off) == 0x30444F4Du) {
                mod0_va = mod_base + off;
                break;
            }
        }
        if (!mod0_va) return false;

        // MOD0 layout: magic(4), dyn_offset(4), bss_start(4), bss_end(4)
        // dyn_offset is relative to the MOD0 header itself.
        const u32 dyn_rel_off = mem.Read32(mod0_va + 4);
        const u64 dyn_va = mod0_va + dyn_rel_off;

        constexpr u32 DT_NULL = 0, DT_PLTRELSZ = 2, DT_STRTAB = 5, DT_SYMTAB = 6, DT_RELA = 7,
                       DT_RELASZ = 8, DT_RELAENT = 9, DT_PLTREL = 20, DT_JMPREL = 23,
                       DT_REL_TAG = 17;
        out.mod_base = mod_base;
        u64 pltrel_kind = DT_RELA; // default per AArch64 ABI (RELA, not REL)
        for (u64 p = dyn_va; ; p += 16) {
            const u64 tag = mem.Read64(p);
            const u64 val = mem.Read64(p + 8);
            if (tag == DT_NULL) break;
            if (tag == DT_RELA)     out.rela_va    = mod_base + val;
            if (tag == DT_RELASZ)   { out.rela_sz = val; out.rela_sz_va = p + 8; }
            if (tag == DT_RELAENT)  out.rela_ent   = val;
            if (tag == DT_JMPREL)   out.jmprel_va  = mod_base + val;
            if (tag == DT_PLTRELSZ) { out.jmprel_sz = val; out.jmprel_sz_va = p + 8; }
            if (tag == DT_PLTREL)   pltrel_kind    = val;
            if (tag == DT_SYMTAB)   out.symtab_va  = mod_base + val;
            if (tag == DT_STRTAB)   out.strtab_va  = mod_base + val;
            if (p - dyn_va > 0x1000) break; // safety
        }
        // DT_PLTREL says whether JMPREL uses 16-byte REL entries (no addend)
        // instead of 24-byte RELA - vanishingly rare on AArch64, but assuming
        // RELA unconditionally would silently misalign every read if a
        // module did use it. Checked after the loop since DT_PLTREL can
        // appear either before or after the entries it describes.
        out.jmprel_ent = (pltrel_kind == DT_REL_TAG) ? 16 : 24;
        return true;
    }

    // Read a symbol's name (from .dynstr) and value for GLOB_DAT/JUMP_SLOT
    // resolution. Elf64_Sym: st_name(4) st_info(1) st_other(1) st_shndx(2)
    // st_value(8) st_size(8) = 24 bytes.
    struct SymInfo {
        std::string name;
        u64 value = 0;
        bool defined = false;
    };
    SymInfo ReadSymbol(const DynInfo& d, u32 index) {
        auto& mem = system.ApplicationMemory();
        SymInfo s;
        if (!d.symtab_va) return s;
        const u64 sym_va = d.symtab_va + static_cast<u64>(index) * 24;
        const u32 name_off = mem.Read32(sym_va);
        // st_shndx is a 2-byte field at offset 6 (st_name(4) st_info(1)
        // st_other(1) st_shndx(2) st_value(8) st_size(8)) - reading 4 bytes
        // here previously spilled into st_value's low bytes, corrupting the
        // defined/undefined check for essentially every symbol whose value
        // had nonzero low 16 bits.
        const u16 shndx = mem.Read16(sym_va + 6);
        s.value = mem.Read64(sym_va + 8);
        s.defined = shndx != 0; // SHN_UNDEF == 0
        if (d.strtab_va) {
            std::string name;
            for (u64 i = 0; i < 512; ++i) {
                const u8 c = static_cast<u8>(mem.Read8(d.strtab_va + name_off + i));
                if (!c) break;
                name.push_back(static_cast<char>(c));
            }
            s.name = std::move(name);
        }
        return s;
    }

    // Every module's exported (defined) symbols, keyed by name, so
    // GLOB_DAT/JUMP_SLOT relocations that reference another module's symbol
    // (e.g. main calling into sdk, or rtld exporting to everything) can be
    // resolved. Built once, across every module, before any relocation
    // actually writes anything - a relocation processed before its target
    // module's exports are indexed would silently resolve to nothing.
    void IndexExports(const DynInfo& d, std::unordered_map<std::string, u64>& out) {
        if (!d.symtab_va || !d.strtab_va) return;
        // No count is stored in .dynamic for a plain DT_SYMTAB (that's normally
        // DT_HASH/DT_GNU_HASH territory), but .dynsym and .dynstr are laid out
        // back to back in every Switch module observed so far, so the gap
        // between them is a reliable entry count - far more so than guessing
        // from name-offset values, which was cutting exports short before
        // rtld's own required symbols were reached (18 unresolved externals
        // for rtld itself were enough to trigger its self-abort).
        u32 max_index = 8192;
        if (d.strtab_va > d.symtab_va) {
            const u64 span = d.strtab_va - d.symtab_va;
            max_index = std::min<u64>(span / 24, 65536);
        }
        for (u32 i = 1; i < max_index; ++i) { // index 0 is always the null symbol
            const auto sym = ReadSymbol(d, i);
            if (sym.defined && !sym.name.empty()) {
                out.emplace(sym.name, d.mod_base + sym.value);
            }
        }
    }

    void ApplyRelocTable(const DynInfo& d, u64 table_va, u64 table_sz, u64 entry_sz,
                          const std::unordered_map<std::string, u64>& exports, u32& applied,
                          u32& unresolved) {
        auto& mem = system.ApplicationMemory();
        constexpr u32 R_AARCH64_ABS64 = 0x101, R_AARCH64_RELATIVE = 0x403,
                       R_AARCH64_GLOB_DAT = 0x401, R_AARCH64_JUMP_SLOT = 0x402,
                       R_AARCH64_IRELATIVE = 0x408;
        for (u64 p = table_va; p < table_va + table_sz; p += entry_sz) {
            const u64 r_offset = mem.Read64(p);
            const u64 r_info   = mem.Read64(p + 8);
            const u64 r_addend = mem.Read64(p + 16);
            const u32 r_type = static_cast<u32>(r_info & 0xFFFFFFFF);
            const u32 r_sym  = static_cast<u32>(r_info >> 32);
            if (r_type == R_AARCH64_RELATIVE) {
                mem.Write64(d.mod_base + r_offset, d.mod_base + r_addend);
                ++applied;
            } else if (r_type == R_AARCH64_IRELATIVE) {
                // IRELATIVE (ifunc): r_addend is a RESOLVER function's address,
                // not the final target - the correct behaviour is to call it
                // (no args, AAPCS64) and store whatever it returns. Actually
                // invoking guest code from inside relocation application would
                // need a full nested-call machinery this backend doesn't have,
                // so - same as a genuinely-unresolved import - patch to the
                // trap sentinel instead of leaving the GOT slot as raw
                // pre-relocation file content. Previously this relocation type
                // matched none of the branches below and was silently skipped
                // entirely, which is exactly how a BLR through this slot ended
                // up jumping to a small leftover file value (e.g. 0xe7ff0)
                // instead of either a real function or a diagnosable trap.
                LOG_ERROR(Core_ARM,
                          "recomp: IRELATIVE relocation at module base={:#x} offset={:#x} not "
                          "invoked (resolver call unsupported); trapped instead",
                          d.mod_base, r_offset);
                mem.Write64(d.mod_base + r_offset, d.trap_va ? d.trap_va : kUnresolvedImportTrap);
            } else if (r_type == R_AARCH64_GLOB_DAT || r_type == R_AARCH64_JUMP_SLOT ||
                       r_type == R_AARCH64_ABS64) {
                const auto sym = ReadSymbol(d, r_sym);
                // ABS64 is S + A, unlike GLOB_DAT/JUMP_SLOT which are plain S.
                // It is by far the most common relocation in a C++ module's
                // .data.rel.ro - every vtable slot, every typeinfo pointer, every
                // static function-pointer table is one - and skipping it left
                // those slots holding the raw module-relative symbol value the
                // linker wrote. A virtual call through such a vtable branches to
                // that small offset instead of base+offset, which is unmapped:
                // that is the whole "cannot execute instruction at unmapped
                // address 0xe7ff0" family of boot crashes.
                const u64 addend = (r_type == R_AARCH64_ABS64) ? r_addend : 0;
                // Linker-synthesized section-boundary symbols (__got_start,
                // __rela_dyn_end, __tbss_align_abs, __EX_start, etc.) describe
                // the CURRENT module's own layout - they're self-referential,
                // not imports - but the minimal Switch toolchain often leaves
                // them marked SHN_UNDEF anyway despite carrying a correct
                // st_value. A nonzero value on an otherwise-"undefined"
                // symbol is a strong signal it's one of these, not a genuine
                // external import (those are left at value 0 with nothing to
                // point to), so trust it ahead of both the defined check and
                // cross-module export lookup.
                u64 synthetic = 0;
                bool is_synthetic = true;
                // These describe THIS module's own relocation sections - the
                // linker leaves them SHN_UNDEF/value-0 in the dynamic symbol
                // table expecting the loader to patch them in directly from
                // its own knowledge of where it placed .rela.dyn/.rela.plt,
                // rather than resolving them like a normal import. We already
                // parsed those bounds for our own use.
                if (sym.name == "__rela_dyn_start" || sym.name == "__rel_dyn_start") {
                    synthetic = d.rela_va;
                } else if (sym.name == "__rela_dyn_end" || sym.name == "__rel_dyn_end") {
                    synthetic = d.rela_va + d.rela_sz;
                } else if (sym.name == "__rela_plt_start" || sym.name == "__rel_plt_start") {
                    synthetic = d.jmprel_va;
                } else if (sym.name == "__rela_plt_end" || sym.name == "__rel_plt_end") {
                    synthetic = d.jmprel_va + d.jmprel_sz;
                } else {
                    is_synthetic = false;
                }
                if (is_synthetic && synthetic) {
                    mem.Write64(d.mod_base + r_offset, synthetic + addend);
                    ++applied;
                } else if (sym.defined || sym.value != 0) {
                    mem.Write64(d.mod_base + r_offset, d.mod_base + sym.value + addend);
                    ++applied;
                } else if (auto it = exports.find(sym.name); it != exports.end()) {
                    mem.Write64(d.mod_base + r_offset, it->second + addend);
                    ++applied;
                } else if (r_type == R_AARCH64_ABS64 && r_sym == 0) {
                    // STN_UNDEF ABS64: S is 0 by definition, so the result is
                    // the addend alone - a plain absolute constant, not a
                    // failed import. Never trap these.
                    mem.Write64(d.mod_base + r_offset, addend);
                    ++applied;
                } else {
                    ++unresolved;
                    if (unresolved <= 30) {
                        LOG_ERROR(Core_ARM, "recomp: unresolved GOT/PLT symbol '{}' for module base={:#x}",
                                  sym.name.empty() ? "<no name>" : sym.name, d.mod_base);
                    }
                    // Patch to the trap sentinel rather than leaving the slot
                    // as whatever the raw file had - see kUnresolvedImportTrap.
                    mem.Write64(d.mod_base + r_offset, d.trap_va ? d.trap_va : kUnresolvedImportTrap);
                }
            }
        }
    }

    // Pre-apply relocations for every loaded module. Under dynarmic, rtld
    // runs its own self-relocation loop correctly; under ArmRecomp the
    // recompiled loop exits early, leaving most relocations un-applied and
    // corrupting both data reads (R_AARCH64_RELATIVE, e.g. vtables, GOT
    // pointers to local data) and indirect calls through the GOT/PLT
    // (R_AARCH64_GLOB_DAT / R_AARCH64_JUMP_SLOT - unresolved, these are the
    // null/garbage function pointers that were previously observed crashing
    // rtld's module bootstrap). All module bases are already known by the
    // time this runs (the loader maps every NSO up front), so cross-module
    // symbol resolution just needs every module's exports indexed first.
    void ApplyAllRelocations(const std::map<u64, std::string>& all_modules) {
        auto& mem = system.ApplicationMemory();
        std::vector<DynInfo> dyns;
        std::unordered_map<std::string, u64> exports;
        for (const auto& [module_base, name] : all_modules) {
            DynInfo d;
            if (ParseDynamic(module_base, d)) {
                d.trap_va = FindReturnStub(module_base);
                IndexExports(d, exports);
                dyns.push_back(d);
            }
        }
        for (const auto& d : dyns) {
            u32 applied = 0, unresolved = 0;
            if (d.rela_va && d.rela_sz) {
                ApplyRelocTable(d, d.rela_va, d.rela_sz, d.rela_ent, exports, applied, unresolved);
            }
            if (d.jmprel_va && d.jmprel_sz) {
                ApplyRelocTable(d, d.jmprel_va, d.jmprel_sz, d.jmprel_ent, exports, applied,
                                 unresolved);
            }
            // Zero DT_RELASZ/DT_PLTRELSZ so rtld's own self-relocator sees
            // nothing left to do and skips both tables - it runs its own
            // GLOB_DAT/JUMP_SLOT resolution loop with a load-bias consistency
            // check that assumes it's relocating fresh, unresolved entries;
            // finding them already resolved by us trips that check and it
            // calls svcBreak, which is what was hanging every recompiled
            // game at boot despite relocations succeeding.
            if (d.rela_sz_va) mem.Write64(d.rela_sz_va, 0);
            if (d.jmprel_sz_va) mem.Write64(d.jmprel_sz_va, 0);
            LOG_INFO(Core_ARM,
                     "recomp: pre-applied {} relocations ({} unresolved external symbols) for "
                     "module base={:#x}",
                     applied, unresolved, d.mod_base);
        }
    }

    System& system;
    RecompLookupFn lookup{};
    GuestContextView ctx{};
    RecompHostMem bridge{};
    std::atomic<bool> interrupted{false};
    Loader::AppLoader::Modules modules{};
    bool modules_read{false};
    bool rela_applied{false};
    static constexpr size_t kTrail = 32;
    u64 trail[kTrail]{};
    size_t trail_pos{0};

    // Interpreter fallback for PCs the static pass never covered. Built on the
    // first miss rather than up front: most runs never need it, and a JIT per
    // core costs a code cache each.
    Kernel::KProcess* owner_process{};
    DynarmicExclusiveMonitor* exclusive_monitor{};
    std::size_t core_index{};
    bool uses_wall_clock{};
    std::unique_ptr<ArmDynarmic64> fallback{};
    bool in_fallback{false};
    bool fallback_unavailable{false};
};

ArmRecomp::ArmRecomp(System& system, bool uses_wall_clock, RecompLookupFn lookup,
                     Kernel::KProcess* process, DynarmicExclusiveMonitor* exclusive_monitor,
                     std::size_t core_index)
    : ArmInterface{uses_wall_clock}, impl{std::make_unique<Impl>(system, lookup)} {
    impl->owner_process = process;
    impl->exclusive_monitor = exclusive_monitor;
    impl->core_index = core_index;
    impl->uses_wall_clock = uses_wall_clock;
}

ArmRecomp::~ArmRecomp() = default;

bool ArmRecomp::EnterFallback() {
    if (impl->fallback_unavailable) {
        return false;
    }
    if (!impl->fallback) {
        if (!impl->owner_process || !impl->exclusive_monitor) {
            impl->fallback_unavailable = true;
            return false;
        }
        impl->fallback = std::make_unique<ArmDynarmic64>(impl->system, impl->uses_wall_clock,
                                                         impl->owner_process, *impl->exclusive_monitor,
                                                         impl->core_index);
        LOG_WARNING(Core_ARM, "recomp: created JIT fallback for uncovered code");
    }
    impl->in_fallback = true;
    return true;
}

HaltReason ArmRecomp::RunFallback(Kernel::KThread* thread) {
    // The recompiled context is the single source of truth; the JIT is loaded
    // from it on the way in and drained back on the way out, so every accessor
    // on this interface (SVC arguments, thread context save/restore) keeps
    // working unchanged no matter which engine actually ran.
    impl->ctx.pending_svc = kNoPendingSvc;
    impl->ctx.halted = 0;
    impl->interrupted.store(false, std::memory_order_relaxed);

    Kernel::Svc::ThreadContext tctx{};
    this->GetContext(tctx);
    impl->fallback->SetContext(tctx);
    impl->fallback->SetTpidrroEl0(impl->ctx.tpidrro_el0);

    const HaltReason hr = impl->fallback->RunThread(thread);

    impl->fallback->GetContext(tctx);
    this->SetContext(tctx);
    if (True(hr & HaltReason::SupervisorCall)) {
        impl->ctx.pending_svc = impl->fallback->GetSvcNumber();
    }

    // Return to recompiled execution as soon as the PC is covered again, so a
    // single uncovered function costs only the time spent inside it.
    if (impl->lookup && impl->lookup(impl->ctx.pc)) {
        impl->in_fallback = false;
    }
    return hr;
}

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
        impl->ApplyAllRelocations(impl->modules);
    }

    // A previous miss handed this thread to the JIT; keep running there until
    // the PC lands back inside recompiled code. The trap sentinel has to be
    // caught before that hand-off as well as inside the dispatch loop below -
    // a thread already in the JIT that calls an unresolved import would
    // otherwise be handed the sentinel address to execute, which is unmapped.
    if (impl->ctx.pc == kUnresolvedImportTrap) {
        impl->ctx.x[0] = 0;
        impl->ctx.pc = impl->ctx.x[30];
        impl->in_fallback = false;
    }
    if (impl->in_fallback) {
        return RunFallback(thread);
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

        if (impl->ctx.pc == kUnresolvedImportTrap) {
            // Landed here via a BLR through a GOT/JUMP_SLOT slot patched by
            // ApplyRelocTable because no definition was found anywhere - most
            // such imports exist for code paths this particular boot never
            // takes, so treat the call as a no-op: return to the caller with
            // a zeroed result register rather than crash the thread. x30 is
            // the guest's own return address (BLR sets it before the branch),
            // exactly as if this were a real function that did nothing.
            static std::atomic<int> trap_count{0};
            if (trap_count.fetch_add(1, std::memory_order_relaxed) < 16) {
                LOG_ERROR(Core_ARM, "recomp: called through unresolved import (returning to caller {:#x})",
                          impl->ctx.x[30]);
            }
            impl->ctx.x[0] = 0;
            impl->ctx.pc = impl->ctx.x[30];
            continue;
        }

        RecompBlockFn block = impl->lookup(impl->ctx.pc);
        // Test hook: forces every lookup past the Nth to miss, so the JIT
        // fallback below can be exercised on a title that would otherwise never
        // hit a gap. Unset in normal runs.
        {
            static const char* const force_miss = std::getenv("SUYU_RECOMP_FORCE_MISS_AFTER");
            static std::atomic<int> blocks_run{0};
            if (force_miss) {
                const int n = blocks_run.fetch_add(1, std::memory_order_relaxed);
                const int limit = std::atoi(force_miss);
                // Name the blocks either side of the cutoff. Bisecting on the
                // cutoff tells you which index first breaks the run; this turns
                // that index into the actual guest address to look at.
                if (n >= limit - 4 && n <= limit + 4) {
                    LOG_ERROR(Core_ARM, "recomp: block #{} pc={:#x}", n, impl->ctx.pc);
                }
                if (n >= limit) {
                    block = nullptr;
                }
            }
        }
        // A miss is now recoverable, so it can happen many times per second;
        // the full diagnostic dump is kept for the first few only, where it is
        // still useful for finding which indirect call went uncovered.
        static std::atomic<int> miss_count{0};
        const int miss_index = block ? 0 : miss_count.fetch_add(1, std::memory_order_relaxed);
        if (!block && miss_index < 8) {
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
            // No recompiled block covers this address: an indirect branch into
            // code the static pass never reached. The guest's own instructions
            // are still mapped in guest memory, so hand the thread to a JIT and
            // keep going instead of returning PrefetchAbort - that halt reason
            // makes the kernel suspend the thread for a debugger that is not
            // attached, which is a permanent, silent black-screen hang.
            if (miss_index < 64) {
                LOG_ERROR(Core_ARM, "No recompiled block at PC {:#x}; falling back to JIT",
                          impl->ctx.pc);
            } else {
                LOG_DEBUG(Core_ARM, "No recompiled block at PC {:#x}; falling back to JIT",
                          impl->ctx.pc);
            }
            if (!EnterFallback()) {
                LOG_CRITICAL(Core_ARM,
                             "recomp: no JIT fallback available at PC {:#x}; thread cannot "
                             "continue",
                             impl->ctx.pc);
                return HaltReason::PrefetchAbort;
            }
            return RunFallback(thread);
        }

        block(&impl->ctx);

        // The block stopped on an instruction the decoder has no translation
        // for, having parked the PC on that instruction. Running it on the JIT
        // instead keeps guest state exact: the alternative the generated code
        // used to take - step over it and zero x0 - silently produced a
        // plausible-looking null that only surfaced as a crash much later, in
        // whatever code eventually dereferenced it.
        if (impl->ctx.halted == kHaltUnhandled) {
            impl->ctx.halted = 0;
            static std::atomic<int> unhandled_count{0};
            if (unhandled_count.fetch_add(1, std::memory_order_relaxed) < 16) {
                LOG_WARNING(Core_ARM, "recomp: unimplemented opcode at {:#x}; running on JIT",
                            impl->ctx.pc);
            }
            if (!EnterFallback()) {
                LOG_CRITICAL(Core_ARM, "recomp: unimplemented opcode at {:#x} and no JIT fallback",
                             impl->ctx.pc);
                return HaltReason::PrefetchAbort;
            }
            return RunFallback(thread);
        }

        if (impl->ctx.pending_svc != kNoPendingSvc) {
            // Log every SVC call from rtld (first few hundred only to avoid spam)
            LOG_TRACE(Core_ARM, "recomp SVC {} at pc={:#x} x0={:#x} x1={:#x} x2={:#x} x3={:#x}",
                      impl->ctx.pending_svc, impl->ctx.pc, impl->ctx.x[0], impl->ctx.x[1],
                      impl->ctx.x[2], impl->ctx.x[3]);
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
    // Only the guest-owned thread pointer travels in ThreadContext. The
    // read-only one is republished separately by PhysicalCore::LoadContext
    // on every switch-in, so writing it from here would overwrite the
    // kernel's TLS pointer with the guest's.
    impl->ctx.tpidr_el0 = ctx.tpidr;
}

void ArmRecomp::SetTpidrroEl0(u64 value) {
    // The emitted MRS handler for TPIDRRO_EL0 reads this out of the guest
    // context, so it has to land there and nowhere else: writing it into
    // tpidr_el0 (as this used to) destroyed the guest's own thread pointer on
    // every context switch and made the guest emit its IPC header outside the
    // TLS region the kernel parses it from.
    impl->ctx.tpidrro_el0 = value;
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
    // While the JIT is running this thread it is the one that has to be woken;
    // the flag above is only read by the recompiled dispatch loop.
    if (impl->fallback) {
        impl->fallback->SignalInterrupt(thread);
    }
}

const Kernel::DebugWatchpoint* ArmRecomp::HaltedWatchpoint() const {
    return nullptr;
}

void ArmRecomp::RewindBreakpointInstruction() {
    // No breakpoint patching in statically recompiled code.
}

} // namespace Core
