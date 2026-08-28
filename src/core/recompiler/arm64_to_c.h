// SPDX-FileCopyrightText: Copyright 2026 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Header-only AArch64 -> portable C static recompiler engine, shared by the standalone
// tools/static_recompiler CLI and the in-app game export feature.
//
// It decodes a subset of user-mode AArch64 and emits C against a GuestContext (N64Recomp-style).
// Every instruction either translates to native C or emits a runtime fallback, so the generated
// project ALWAYS builds into a native binary (Windows .exe / Linux+BSD ELF / macOS Mach-O) or can
// be emitted as plain C source. A full game additionally needs suyu's HLE/GPU runtime, wired in via
// the generated runtime's recomp_svc()/MMIO hooks.

#pragma once

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

namespace suyu::recomp {

using u8 = uint8_t;
using u32 = uint32_t;
using u64 = uint64_t;
using s32 = int32_t;
using s64 = int64_t;

// Output paths reach this engine as UTF-8 (the frontend hands over
// QString::toStdString()), but the narrow char overloads of _mkdir/mkdir and
// std::ofstream interpret their argument in the process code page - CP-1252 on
// a stock Windows install. A title whose name carries a non-ASCII character
// ("Pokemon Sword" reads as "Pokémon Sword" once the name comes from the
// ROM's own NACP) therefore had every directory creation and every file open
// silently fail, leaving empty module directories and a build that could not
// configure. Route both through std::filesystem::path, which is constructed
// from the UTF-8 bytes explicitly and uses the wide OS entry points.
inline std::filesystem::path Utf8Path(const std::string& s) {
    return std::filesystem::path(
        std::u8string(reinterpret_cast<const char8_t*>(s.data()), s.size()));
}

struct Block {
    u64 vaddr;
    u32 size;
    u32 count;
    bool is_entry;
};

inline bool IsTerminator(u32 i) {
    if ((i & 0xFC000000) == 0x14000000) return true; // B
    if ((i & 0xFC000000) == 0x94000000) return true; // BL
    if ((i & 0xFFFFFC1F) == 0xD61F0000) return true; // BR
    if ((i & 0xFFFFFC1F) == 0xD63F0000) return true; // BLR
    if ((i & 0xFFFFFC1F) == 0xD65F0000) return true; // RET
    if ((i & 0x7F000000) == 0x34000000) return true; // CBZ
    if ((i & 0x7F000000) == 0x35000000) return true; // CBNZ
    if ((i & 0x7F000000) == 0x36000000) return true; // TBZ
    if ((i & 0x7F000000) == 0x37000000) return true; // TBNZ
    if ((i & 0xFF000010) == 0x54000000) return true; // B.cond
    if ((i & 0xFFE0001F) == 0xD4000001) return true; // SVC
    return false;
}

// Statically known target of a direct branch, if the instruction has one.
//
// This has to cover every form whose target the translator bakes in, not just
// the unconditional imm26 pair: a conditional branch's target is equally
// static, and it is overwhelmingly the common case for loop back-edges. Leaving
// B.cond/CBZ/CBNZ/TBZ/TBNZ out here meant their targets never began a block, so
// the generated code would set pc to an address that recomp_lookup - which
// matches block starts exactly - could not resolve, and execution died with
// "No recompiled block at PC" at the top of the first loop it entered.
inline bool DirectBranchTarget(u32 i, u64 pc, u64& out) {
    if ((i & 0xFC000000) == 0x14000000 || (i & 0xFC000000) == 0x94000000) { // B / BL
        s32 imm26 = (s32)(i << 6) >> 6;
        out = pc + (s64)imm26 * 4;
        return true;
    }
    if ((i & 0xFF000010) == 0x54000000 ||    // B.cond
        (i & 0x7E000000) == 0x34000000) {    // CBZ / CBNZ
        s64 imm19 = ((s32)(((i >> 5) & 0x7FFFF) << 13) >> 13);
        out = pc + imm19 * 4;
        return true;
    }
    if ((i & 0x7E000000) == 0x36000000) {    // TBZ / TBNZ
        s64 imm14 = ((s32)(((i >> 5) & 0x3FFF) << 18) >> 18);
        out = pc + imm14 * 4;
        return true;
    }
    return false;
}

// Function addresses the code builds for itself with ADRP+ADD.
//
// A callback handed to the OS - a thread entry above all - is never branched to
// from inside .text and never appears in a relocation either: the compiler just
// materialises its address into a register (adrp xN, page; add xN, xN, #lo12)
// and passes it to svcCreateThread. Without seeding those, every thread the
// game starts begins at an address no block covers and drops straight to the
// interpreter for its whole life, which is most of the "No recompiled block at
// PC" traffic in a real run.
//
// Deliberately loose: any ADRP+ADD landing in .text becomes a root. A pair that
// was really computing a data address only costs one extra block boundary.
inline void CollectAdrpAddTargets(const u8* text, size_t n_bytes, u64 base,
                                  std::vector<u64>& out) {
    const u32 n = static_cast<u32>(n_bytes / 4);
    const u32* p = reinterpret_cast<const u32*>(text);
    // Last ADRP seen per destination register, as a page address; ~0 means the
    // register no longer holds one.
    std::array<u64, 32> page{};
    page.fill(~0ULL);
    for (u32 i = 0; i < n; ++i) {
        const u32 insn = p[i];
        if ((insn & 0x9F000000) == 0x90000000) { // ADRP
            const u32 rd = insn & 31;
            s64 immhi = static_cast<s32>((((insn >> 5) & 0x7FFFF) << 13)) >> 13;
            const u32 immlo = (insn >> 29) & 3;
            page[rd] = ((base + static_cast<u64>(i) * 4) & ~0xFFFULL) +
                       static_cast<u64>(((immhi << 2) | immlo) << 12);
        } else if ((insn & 0xFFC00000) == 0x91000000) { // ADD (immediate, 64-bit, LSL #0)
            const u32 rd = insn & 31, rn = (insn >> 5) & 31;
            const u32 imm12 = (insn >> 10) & 0xFFF;
            if (page[rn] != ~0ULL) {
                const u64 target = page[rn] + imm12;
                if (target >= base && (target - base) / 4 < n && (target & 3) == 0) {
                    out.push_back(target);
                }
            }
            if (rd != rn) {
                page[rd] = ~0ULL;
            }
        } else {
            // Any other write to a register invalidates the page it held. Only
            // the common destination encodings are decoded here; missing one
            // can add a stale root, never remove a real one.
            const u32 rd = insn & 31;
            page[rd] = ~0ULL;
        }
    }
}

inline std::vector<Block> DiscoverBlocks(const u8* text, size_t n_bytes, u64 base,
                                         u64 entry_pc = 0,
                                         const std::vector<u64>* extra_roots = nullptr) {
    const u32 n = (u32)(n_bytes / 4);
    if (n == 0) return {};
    std::vector<bool> start(n, false);
    start[0] = true;
    // The module entry has to begin a block in its own right. Nothing branches
    // to it from inside the image, and for a real NSO it isn't offset 0
    // either - .text opens with a MOD0 header - so without this the entry
    // address falls in the middle of some other block and lookup fails.
    if (entry_pc > base && (entry_pc - base) / 4 < n) {
        start[(u32)((entry_pc - base) / 4)] = true;
    }
    // Exported dynsym addresses (functions like nn::init::Start, only ever
    // reached indirectly via another module's resolved GOT/PLT entry, never
    // by a direct branch inside this module's own .text). Without seeding
    // these as roots too, a function preceded by alignment padding rather
    // than a terminator falls mid-block and the runtime dispatcher can never
    // resolve a call landing exactly on its real entry address.
    if (extra_roots) {
        for (u64 addr : *extra_roots) {
            if (addr >= base && (addr - base) / 4 < n) {
                start[(u32)((addr - base) / 4)] = true;
            }
        }
    }
    {
        std::vector<u64> computed;
        CollectAdrpAddTargets(text, n_bytes, base, computed);
        for (u64 addr : computed) {
            start[(u32)((addr - base) / 4)] = true;
        }
    }
    const u32* p = reinterpret_cast<const u32*>(text);
    for (u32 i = 0; i < n; ++i) {
        const u32 insn = p[i];
        const u64 pc = base + (u64)i * 4;
        if (IsTerminator(insn)) {
            if (i + 1 < n) start[i + 1] = true;
            u64 t = 0;
            if (DirectBranchTarget(insn, pc, t) && t >= base && (t - base) / 4 < n)
                start[(u32)((t - base) / 4)] = true;
        }
    }
    std::vector<Block> blocks;
    u32 s = 0;
    for (u32 i = 1; i <= n; ++i) {
        if (i == n || start[i]) {
            blocks.push_back(Block{base + (u64)s * 4, (i - s) * 4, i - s, s == 0});
            s = i;
        }
    }
    return blocks;
}

// ARM's logical-immediate encoding: N:immr:imms describe a repeating bit
// pattern rather than a literal value. This is the standard DecodeBitMasks
// from the architecture reference, restricted to the immediate (non-tested)
// result. Returns false for the reserved encodings.
inline bool DecodeBitMasks(u32 N, u32 imms, u32 immr, bool is64, u64& out) {
    if (!is64 && N) return false;
    // len = index of the highest set bit of (N:~imms), computed portably.
    const u32 bits = (N << 6) | ((~imms) & 0x3F);
    if (bits == 0) return false;
    u32 len = 0;
    for (u32 t = bits; t > 1; t >>= 1) ++len;
    if (len < 1) return false;
    const u32 esize = 1u << len;
    if (esize > (is64 ? 64u : 32u)) return false;
    const u32 levels = esize - 1;
    const u32 s = imms & levels;
    const u32 r = immr & levels;
    if (s == levels) return false;   // reserved
    u64 welem = (s + 1 >= 64) ? ~0ULL : ((1ULL << (s + 1)) - 1);
    // Rotate right within the element, then replicate to the register width.
    // The rotate applies at every element size, 64 included: skipping it there
    // (to dodge the undefined `welem << 64` when r is 0) silently turned every
    // 64-bit rotated mask into its unrotated form - so "and x8, x8, #~0xF",
    // the standard align-down, decoded as `& 0x0FFFFFFFFFFFFFFF` and left the
    // pointer unaligned instead. Guard r == 0 explicitly instead.
    u64 elem;
    if (r == 0) {
        elem = welem;
    } else if (esize >= 64) {
        elem = (welem >> r) | (welem << (64 - r));
    } else {
        elem = ((welem >> r) | (welem << (esize - r))) & ((1ULL << esize) - 1);
    }
    u64 result = 0;
    for (u32 i = 0; i < (is64 ? 64u : 32u); i += esize) result |= elem << i;
    if (!is64) result &= 0xFFFFFFFFULL;
    out = result;
    return true;
}

inline std::string Xz(u32 r) {
    return r == 31 ? std::string("(uint64_t)0") : ("c->x[" + std::to_string(r) + "]");
}
inline std::string Wz(u32 r) {
    return r == 31 ? std::string("(uint32_t)0") : ("(uint32_t)c->x[" + std::to_string(r) + "]");
}

/// Register 31 as the stack pointer rather than the zero register.
///
/// AArch64 spells both with the same encoding and the meaning depends on the
/// instruction: ADD/SUB with an immediate or an extended register read it as
/// SP, while the shifted-register and logical forms read it as XZR. Using the
/// zero-register spelling everywhere makes every function prologue -
/// "sub sp, sp, #N", "add x29, sp, #N" - compute from zero, so the frame lands
/// at a tiny address and every local access writes into unmapped memory near
/// null.
inline std::string Xsp(u32 r) {
    return "c->x[" + std::to_string(r) + "]";
}

// Append C for one instruction. Returns false if the instruction terminates the block.
inline bool Translate(u32 i, u64 pc, std::string& out) {
    char buf[256];
    // Appends in place. Taking a string_view and appending piecewise (rather
    // than `out += "    " + s + "\n"`) matters at scale: that expression built
    // two temporary std::strings per emitted line, and a full title is tens of
    // millions of lines - it was the single largest cost in AOT translation.
    auto put = [&](std::string_view s) {
        out.append("    ", 4);
        out.append(s);
        out.push_back('\n');
    };
    const u64 next = pc + 4;

    if ((i & 0xFFFFF01F) == 0xD503201F) { put("/* nop/hint */"); return true; }

    // Memory barriers: DSB, DMB, ISB and CLREX. The generated runtime executes
    // one guest thread on one host thread, so there is no other observer for
    // these to order against and nothing to synchronise - they are genuinely
    // no-ops here. Under Core::ArmRecomp, where real threads exist, these need
    // the host's own barriers instead; noted so the assumption stays visible.
    // Emitting nothing at all is only right for the standalone runtime, which
    // drives one guest thread on one host thread and so has no second observer
    // to order against. Under Core::ArmRecomp the guest really is
    // multi-threaded, and a comment does not stop the host compiler reordering
    // the recomp_load/recomp_store calls around it - so a publish pattern (fill
    // an object, barrier, store the pointer) can be observed pointer-first by
    // another thread, surfacing much later as a live vtable pointer reading
    // back as zero.
    if ((i & 0xFFFFF01F) == 0xD503301F) { put("recomp_barrier();"); return true; }

    if ((i & 0x1F800000) == 0x12800000) { // MOVZ/MOVN/MOVK
        u32 sf = i >> 31, opc = (i >> 29) & 3, hw = (i >> 21) & 3, imm16 = (i >> 5) & 0xFFFF, rd = i & 31;
        if (rd != 31) {
            u64 shift = (u64)hw * 16;
            if (opc == 2) {
                snprintf(buf, sizeof buf, "c->x[%u] = 0x%llxULL;", rd, (unsigned long long)((u64)imm16 << shift));
                put(buf);
            } else if (opc == 0) {
                u64 v = ~((u64)imm16 << shift); if (!sf) v &= 0xFFFFFFFF;
                snprintf(buf, sizeof buf, "c->x[%u] = 0x%llxULL;", rd, (unsigned long long)v); put(buf);
            } else if (opc == 3) {
                snprintf(buf, sizeof buf, "c->x[%u] = (c->x[%u] & ~(0xFFFFULL<<%llu)) | (0x%xULL<<%llu);",
                         rd, rd, (unsigned long long)shift, imm16, (unsigned long long)shift); put(buf);
            }
            if (!sf) { snprintf(buf, sizeof buf, "c->x[%u] &= 0xFFFFFFFFULL;", rd); put(buf); }
        }
        return true;
    }

    if ((i & 0x1F000000) == 0x11000000) { // ADD/SUB immediate
        u32 sf = i >> 31, op = (i >> 30) & 1, S = (i >> 29) & 1, sh = (i >> 22) & 1;
        u32 imm12 = (i >> 10) & 0xFFF, rn = (i >> 5) & 31, rd = i & 31;
        u64 imm = sh ? ((u64)imm12 << 12) : imm12;
        snprintf(buf, sizeof buf, "{ uint64_t _a=c->x[%u], _b=%lluULL; uint64_t _r=%s; ", rn,
                 (unsigned long long)imm, op ? "_a-_b" : "_a+_b");
        std::string s = buf;
        if (!sf) s += "_r&=0xFFFFFFFFULL; ";
        // With the flag-setting form, register 31 as the destination is the
        // zero register - that encoding is CMP - so the result is dropped.
        // Without it, register 31 is SP and the write is real.
        if (!(rd == 31 && S)) s += "c->x[" + std::to_string(rd) + "]=_r; ";
        if (S) s += "recomp_set_flags(c," + std::string(op ? "1" : "0") + ",_a,_b,_r," + (sf ? "1" : "0") + "); ";
        s += "}";
        // Register 31 is SP here, not the zero register, so a write to it is
        // real and must not be discarded: dropping it throws away every
        // "sub sp, sp, #N" that opens a stack frame.
        put(s);
        return true;
    }

    // Build a shifted-register operand. LSR/ASR/ROR all used to collapse to a
    // plain ">>" on a uint64_t, which zero-fills - so ASR lost the sign (the
    // "bic w0, w0, w0, asr #31" max(x,0) idiom produced ~0 instead of 0) and
    // ROR dropped the wrapped-around bits entirely (breaking, among other
    // things, software CRC32). The 32-bit forms also have to be narrowed
    // before shifting, whatever the shift amount, because the register may
    // still carry high garbage from an earlier 64-bit write.
    auto shifted_operand = [](const std::string& v, u32 shift, u32 imm6, u32 sf) -> std::string {
        char sb[256];
        if (!sf) {
            const std::string w = "((uint32_t)(" + v + "))";
            switch (shift) {
            case 0: snprintf(sb, sizeof sb, "((uint64_t)(uint32_t)(%s << %u))", w.c_str(), imm6); break;
            case 1: snprintf(sb, sizeof sb, "((uint64_t)(%s >> %u))", w.c_str(), imm6); break;
            case 2: snprintf(sb, sizeof sb, "((uint64_t)(uint32_t)((int32_t)%s >> %u))", w.c_str(), imm6); break;
            default:
                if (imm6 == 0) { snprintf(sb, sizeof sb, "((uint64_t)%s)", w.c_str()); }
                else { snprintf(sb, sizeof sb, "((uint64_t)(uint32_t)((%s >> %u) | (%s << %u)))", w.c_str(), imm6, w.c_str(), 32 - imm6); }
                break;
            }
        } else {
            switch (shift) {
            case 0: snprintf(sb, sizeof sb, "((%s) << %u)", v.c_str(), imm6); break;
            case 1: snprintf(sb, sizeof sb, "((%s) >> %u)", v.c_str(), imm6); break;
            case 2: snprintf(sb, sizeof sb, "((uint64_t)((int64_t)(%s) >> %u))", v.c_str(), imm6); break;
            default:
                if (imm6 == 0) { snprintf(sb, sizeof sb, "(%s)", v.c_str()); }
                else { snprintf(sb, sizeof sb, "(((%s) >> %u) | ((%s) << %u))", v.c_str(), imm6, v.c_str(), 64 - imm6); }
                break;
            }
        }
        return sb;
    };

    if ((i & 0x1F000000) == 0x0A000000) { // logical shifted register
        u32 sf = i >> 31, opc = (i >> 29) & 3, rm = (i >> 16) & 31, rn = (i >> 5) & 31, rd = i & 31;
        u32 shift = (i >> 22) & 3, imm6 = (i >> 10) & 0x3F, N = (i >> 21) & 1;
        std::string rmv = shifted_operand(Xz(rm), shift, imm6, sf);
        std::string a = Xz(rn);
        const char* lop = opc == 0 ? "&" : opc == 1 ? "|" : opc == 2 ? "^" : "&";
        std::string expr = N ? ("(" + a + " " + lop + " ~" + rmv + ")") : ("(" + a + " " + lop + " " + rmv + ")");
        // opc==3 is ANDS/BICS: it sets the flags, and with rd==31 it is TST,
        // whose only effect IS the flag update. Gating the whole instruction on
        // rd != 31 discarded every TST, leaving the following b.cond to branch
        // on whatever flags happened to be left over from an earlier compare.
        std::string s = "{ uint64_t _r = " + expr + "; ";
        if (!sf) s += "_r &= 0xFFFFFFFFULL; ";
        if (rd != 31) s += "c->x[" + std::to_string(rd) + "] = _r; ";
        if (opc == 3) s += "recomp_set_flags(c,0,_r,0,_r," + std::string(sf ? "1" : "0") + "); ";
        s += "}";
        put(s);
        return true;
    }

    if ((i & 0x1F200000) == 0x0B000000) { // ADD/SUB shifted register
        u32 sf = i >> 31, op = (i >> 30) & 1, S = (i >> 29) & 1, shift = (i >> 22) & 3, rm = (i >> 16) & 31, imm6 = (i >> 10) & 0x3F, rn = (i >> 5) & 31, rd = i & 31;
        // ROR is reserved for ADD/SUB shifted register - decoding it as a shift
        // would silently invent an instruction the CPU does not have.
        if (shift == 3) {
            snprintf(buf, sizeof buf,
                     "recomp_unhandled(c,0x%08xU,g_module_base+0x%llxULL); if(c->halted) return;", i,
                     (unsigned long long)pc);
            put(buf);
            return true;
        }
        std::string rmv = shifted_operand(Xz(rm), shift, imm6, sf);
        std::string a = Xz(rn);
        snprintf(buf, sizeof buf, "{ uint64_t _a=%s,_b=%s,_r=%s; ", a.c_str(), rmv.c_str(), op ? "_a-_b" : "_a+_b");
        std::string s = buf; if (!sf) s += "_r&=0xFFFFFFFFULL; ";
        if (rd != 31) s += "c->x[" + std::to_string(rd) + "]=_r; ";
        if (S) s += "recomp_set_flags(c," + std::string(op ? "1" : "0") + ",_a,_b,_r," + (sf ? "1" : "0") + "); ";
        s += "}"; put(s); return true;
    }

    if ((i & 0x1F000000) == 0x10000000) { // ADR/ADRP
        u32 op = i >> 31, rd = i & 31; s64 immhi = (s32)(((i >> 5) & 0x7FFFF) << 13) >> 13; u32 immlo = (i >> 29) & 3;
        if (rd != 31) {
            // These produce data pointers the guest then dereferences, so they
            // have to be real addresses. Everything the static pass knows is
            // module-relative, so the module's load base is added at run time -
            // without it every computed pointer lands near null.
            if (op) { u64 b = (pc & ~0xFFFULL); s64 imm = ((immhi << 2) | immlo) << 12; snprintf(buf, sizeof buf, "c->x[%u]=g_module_base + 0x%llxULL + (int64_t)%lld;", rd, (unsigned long long)b, (long long)imm); }
            else { s64 imm = (immhi << 2) | immlo; snprintf(buf, sizeof buf, "c->x[%u]=g_module_base + 0x%llxULL + (int64_t)%lld;", rd, (unsigned long long)pc, (long long)imm); }
            put(buf);
        }
        return true;
    }

    // LDR/STR immediate unsigned offset. Bit 26 is the V bit: it selects the
    // SIMD/FP register file rather than the general registers. Leaving it out
    // of the mask made every "LDR s0, [x1, #8]" compile into an integer load
    // of x0 - silently wrong code rather than an honest fallback. The SIMD
    // form is handled separately further down.
    if ((i & 0x3F000000) == 0x39000000) {
        u32 size = (i >> 30) & 3, opc = (i >> 22) & 3, imm12 = (i >> 10) & 0xFFF, rn = (i >> 5) & 31, rt = i & 31;
        u64 off = (u64)imm12 << size;
        std::string addr = "c->x[" + std::to_string(rn) + "] + " + std::to_string(off);
        const char* ty = size == 0 ? "8" : size == 1 ? "16" : size == 2 ? "32" : "64";
        // opc: 00 store, 01 load (zero-extend), 10 load signed to 64-bit,
        // 11 load signed to 32-bit. Testing (opc & 1) got this wrong in both
        // directions - opc 11 loaded without sign extension, and opc 10 (a
        // *load*) fell into the store branch and wrote to memory instead.
        if (opc == 0) {
            snprintf(buf, sizeof buf, "recomp_store%s(c,%s,%s);", ty, addr.c_str(), Xz(rt).c_str());
            put(buf);
        } else if (opc == 1) {
            if (rt != 31) {
                snprintf(buf, sizeof buf, "c->x[%u]=recomp_load%s(c,%s);", rt, ty, addr.c_str());
                put(buf);
            }
        } else if (size < 3) {
            // Signed load: sign-extend from the accessed width.
            const char* st = size == 0 ? "int8_t" : size == 1 ? "int16_t" : "int32_t";
            if (rt != 31) {
                std::string s = "{ uint64_t _r = (uint64_t)(int64_t)(" + std::string(st) +
                                ")recomp_load" + ty + "(c," + addr + "); ";
                if (opc == 3) s += "_r &= 0xFFFFFFFFULL; ";   // 32-bit destination
                s += "c->x[" + std::to_string(rt) + "] = _r; }";
                put(s);
            }
        } else {
            // size==3 with opc>=2 is not a defined load/store here.
            snprintf(buf, sizeof buf,
                     "recomp_unhandled(c,0x%08xU,g_module_base+0x%llxULL); if(c->halted) return;", i,
                     (unsigned long long)pc);
            put(buf);
            return true;
        }
        return true;
    }

    u64 t = 0;
    // Only the unconditional forms here: DirectBranchTarget also decodes
    // B.cond/CBZ/TBZ for block discovery, but those have their own translations
    // further down and must not be turned into unconditional jumps.
    if (((i & 0xFC000000) == 0x14000000 || (i & 0xFC000000) == 0x94000000) &&
        DirectBranchTarget(i, pc, t)) {
        if ((i & 0xFC000000) == 0x94000000) { snprintf(buf, sizeof buf, "c->x[30]=g_module_base+0x%llxULL;", (unsigned long long)next); put(buf); }
        snprintf(buf, sizeof buf, "c->pc=g_module_base+0x%llxULL; return;", (unsigned long long)t); put(buf); return false;
    }
    if ((i & 0xFFFFFC1F) == 0xD65F0000) { put("c->pc=c->x[30]; return; /* RET */"); return false; }
    if ((i & 0xFFFFFC1F) == 0xD61F0000) { u32 rn = (i >> 5) & 31; snprintf(buf, sizeof buf, "c->pc=c->x[%u]; return; /* BR */", rn); put(buf); return false; }
    if ((i & 0xFFFFFC1F) == 0xD63F0000) { u32 rn = (i >> 5) & 31; snprintf(buf, sizeof buf, "c->x[30]=g_module_base+0x%llxULL; c->pc=c->x[%u]; return; /* BLR */", (unsigned long long)next, rn); put(buf); return false; }
    if ((i & 0xFF000010) == 0x54000000) { s64 off = ((s32)((i >> 5) << 13) >> 13); u64 tt = pc + off * 4; u32 cond = i & 15; snprintf(buf, sizeof buf, "if (recomp_cond(c,%u)) { c->pc=g_module_base+0x%llxULL; } else { c->pc=g_module_base+0x%llxULL; } return;", cond, (unsigned long long)tt, (unsigned long long)next); put(buf); return false; }
    if ((i & 0x7E000000) == 0x34000000) { u32 sf = i >> 31; bool nz = (i >> 24) & 1; u32 rt = i & 31; s64 off = ((s32)(((i >> 5) & 0x7FFFF) << 13) >> 13); u64 tt = pc + off * 4; std::string v = sf ? Xz(rt) : Wz(rt); snprintf(buf, sizeof buf, "if ((%s)%s0) { c->pc=g_module_base+0x%llxULL; } else { c->pc=g_module_base+0x%llxULL; } return;", v.c_str(), nz ? "!=" : "==", (unsigned long long)tt, (unsigned long long)next); put(buf); return false; }
    if ((i & 0x7E000000) == 0x36000000) { bool nz = (i >> 24) & 1; u32 b = ((i >> 31) << 5) | ((i >> 19) & 31); u32 rt = i & 31; s64 off = ((s32)(((i >> 5) & 0x3FFF) << 18) >> 18); u64 tt = pc + off * 4; snprintf(buf, sizeof buf, "if (((%s>>%u)&1)%s0) { c->pc=g_module_base+0x%llxULL; } else { c->pc=g_module_base+0x%llxULL; } return;", Xz(rt).c_str(), b, nz ? "!=" : "==", (unsigned long long)tt, (unsigned long long)next); put(buf); return false; }
    if ((i & 0xFFE0001F) == 0xD4000001) { u32 imm = (i >> 5) & 0xFFFF; snprintf(buf, sizeof buf, "c->pc=g_module_base+0x%llxULL; c->pending_svc=%uULL; recomp_svc(c,%u); return;", (unsigned long long)next, imm, imm); put(buf); return false; }

    // STP/LDP - load/store pair. Every non-leaf AArch64 function opens and
    // closes with these, so without them a real game stops at its first
    // prologue. Covers the signed-offset, pre-index and post-index forms for
    // both 32- and 64-bit operands.
    // Bit 26 (V) must be clear here; the SIMD/FP pair form is handled below.
    if ((i & 0x3E000000) == 0x28000000) {
        const u32 opc = i >> 30;            // 0 = 32-bit, 2 = 64-bit
        const bool is_load = (i >> 22) & 1;
        const u32 mode = (i >> 23) & 3;     // 1 post-index, 2 signed offset, 3 pre-index
        const u32 rt2 = (i >> 10) & 31, rn = (i >> 5) & 31, rt = i & 31;
        s32 imm7 = (s32)((i >> 15) & 0x7F);
        if (imm7 & 0x40) imm7 |= ~0x7F;     // sign-extend 7 bits
        if ((opc == 0 || opc == 2) && mode >= 1 && mode <= 3) {
            const u32 sz = (opc == 2) ? 8 : 4;
            const s64 off = (s64)imm7 * sz;
            std::string s = "{ uint64_t _b=c->x[" + std::to_string(rn) + "]; ";
            // Pre-index and post-index both write the new base back; only
            // pre-index applies the offset before the access.
            const char* addr = (mode == 1) ? "_b" : "(_b+_o)";
            s += "int64_t _o=" + std::to_string((long long)off) + "; ";
            if (is_load) {
                // Rt/Rt2 == 31 is XZR here, so the loaded value is discarded -
                // writing it would land on c->x[31], which is where SP lives.
                if (rt != 31) {
                    s += "c->x[" + std::to_string(rt) + "]=recomp_load" + std::to_string(sz * 8) +
                         "(c," + addr + "); ";
                }
                if (rt2 != 31) {
                    s += "c->x[" + std::to_string(rt2) + "]=recomp_load" + std::to_string(sz * 8) +
                         "(c," + addr + "+" + std::to_string(sz) + "); ";
                }
            } else {
                s += "recomp_store" + std::to_string(sz * 8) + "(c," + addr + "," +
                     (rt == 31 ? std::string("(uint64_t)0") : ("c->x[" + std::to_string(rt) + "]")) + "); ";
                s += "recomp_store" + std::to_string(sz * 8) + "(c," + addr + "+" +
                     std::to_string(sz) + "," +
                     (rt2 == 31 ? std::string("(uint64_t)0") : ("c->x[" + std::to_string(rt2) + "]")) + "); ";
            }
            if (mode == 1 || mode == 3) {
                s += "c->x[" + std::to_string(rn) + "]=_b+_o; ";
            }
            s += "}";
            put(s);
            return true;
        }
    }

    // Logical immediate: AND/ORR/EOR/ANDS. By far the largest single gap in
    // real code - the immediate is a repeating bit pattern, not a literal.
    if ((i & 0x1F800000) == 0x12000000) {
        const u32 sf = i >> 31, opc = (i >> 29) & 3, N = (i >> 22) & 1;
        const u32 immr = (i >> 16) & 0x3F, imms = (i >> 10) & 0x3F;
        const u32 rn = (i >> 5) & 31, rd = i & 31;
        u64 imm = 0;
        if (DecodeBitMasks(N, imms, immr, sf != 0, imm)) {
            const char* op = (opc == 0 || opc == 3) ? "&" : (opc == 1 ? "|" : "^");
            std::string s = "{ uint64_t _r = " + Xz(rn) + " " + op + " 0x" ;
            char hb[32]; snprintf(hb, sizeof hb, "%llxULL", (unsigned long long)imm);
            s += hb; s += "; ";
            if (!sf) s += "_r &= 0xFFFFFFFFULL; ";
            // Rd==31 is SP for AND/ORR/EOR immediate and only reads as XZR
            // for ANDS (opc==3). Treating it as XZR everywhere silently
            // dropped every "and sp, xN, #imm" stack realignment.
            if (!(rd == 31 && opc == 3)) s += "c->x[" + std::to_string(rd) + "] = _r; ";
            if (opc == 3) s += "recomp_set_flags(c,0,_r,0,_r," + std::string(sf ? "1" : "0") + "); ";
            s += "}";
            put(s);
            return true;
        }
    }

    // Bitfield: UBFM/SBFM/BFM - the encoding behind LSL/LSR/ASR/UBFX/SBFX.
    if ((i & 0x1F800000) == 0x13000000) {
        const u32 sf = i >> 31, opc = (i >> 29) & 3;
        const u32 immr = (i >> 16) & 0x3F, imms = (i >> 10) & 0x3F;
        const u32 rn = (i >> 5) & 31, rd = i & 31;
        const u32 width = sf ? 64 : 32;

        // BFM (opc 1) - the encoding behind BFI and BFXIL. Unlike UBFM/SBFM it
        // merges into the destination, leaving the bits outside the inserted
        // field untouched, so it needs an explicit mask rather than a shift.
        if (rd != 31 && opc == 1 && immr < width && imms < width) {
            u32 nbits, src_shift, dst_pos;
            if (imms >= immr) {          // BFXIL: extract to the bottom of Rd
                nbits = imms - immr + 1;
                src_shift = immr;
                dst_pos = 0;
            } else {                     // BFI: insert at (width - immr)
                nbits = imms + 1;
                src_shift = 0;
                dst_pos = width - immr;
            }
            if (nbits + dst_pos <= width) {
                // Build the field mask at 64 bits; nbits is < 64 here because
                // dst_pos + nbits <= width <= 64 and a 64-wide field would
                // make the shift below undefined.
                const u64 field = (nbits >= 64) ? ~0ULL : ((1ULL << nbits) - 1ULL);
                const u64 mask = field << dst_pos;
                char mb[48], fb[48];
                snprintf(mb, sizeof mb, "0x%llxULL", (unsigned long long)mask);
                snprintf(fb, sizeof fb, "0x%llxULL", (unsigned long long)field);
                std::string s = "{ uint64_t _s = " + Xz(rn);
                if (src_shift) s += " >> " + std::to_string(src_shift);
                s += "; uint64_t _r = (c->x[" + std::to_string(rd) + "] & ~" + mb +
                     ") | ((_s & " + fb + ") << " + std::to_string(dst_pos) + "); ";
                if (!sf) s += "_r &= 0xFFFFFFFFULL; ";
                s += "c->x[" + std::to_string(rd) + "] = _r; }";
                put(s);
                return true;
            }
        }

        // UBFM (opc 2) and SBFM (opc 0).
        if (rd != 31 && opc != 1 && immr < width && imms < width) {
            const char* mask = sf ? "" : " & 0xFFFFFFFFULL";
            std::string src = Xz(rn);
            std::string s = "{ uint64_t _s = " + src + mask + "; uint64_t _r; ";
            if (imms >= immr) {
                // Extract (imms-immr+1) bits starting at immr.
                const u32 nbits = imms - immr + 1;
                s += "_r = (_s >> " + std::to_string(immr) + ")";
                if (nbits < 64) s += " & ((1ULL << " + std::to_string(nbits) + ") - 1)";
                s += "; ";
                if (opc == 0) { // SBFM: sign-extend from nbits
                    s += "if (_r & (1ULL << " + std::to_string(nbits - 1) + ")) _r |= ~((1ULL << " +
                         std::to_string(nbits) + ") - 1); ";
                }
            } else {
                // Insert: bits [0..imms] moved to start at (width-immr).
                const u32 nbits = imms + 1;
                const u32 shift = width - immr;
                s += "_r = (_s";
                if (nbits < 64) s += " & ((1ULL << " + std::to_string(nbits) + ") - 1)";
                s += ") << " + std::to_string(shift) + "; ";
                if (opc == 0) {
                    s += "if (_r & (1ULL << " + std::to_string(shift + nbits - 1) +
                         ")) _r |= ~((1ULL << " + std::to_string(shift + nbits) + ") - 1); ";
                }
            }
            if (!sf) s += "_r &= 0xFFFFFFFFULL; ";
            s += "c->x[" + std::to_string(rd) + "] = _r; }";
            put(s);
            return true;
        }
    }

    // Conditional compare: CCMP/CCMN, both the register and 5-bit immediate
    // forms. When the condition holds this behaves as an ordinary compare;
    // when it does not, the flags are loaded straight from the nzcv field.
    // Short-circuit conditionals in C compile to chains of these.
    if ((i & 0x1FE00000) == 0x1A400000 && ((i >> 29) & 1) && ((i >> 10) & 1) == 0 &&
        ((i >> 4) & 1) == 0) {
        const u32 sf = i >> 31, op = (i >> 30) & 1;   // op: 0 = CCMN, 1 = CCMP
        const u32 imm_or_rm = (i >> 16) & 31, cond = (i >> 12) & 15;
        const bool is_imm = ((i >> 11) & 1) != 0;
        const u32 rn = (i >> 5) & 31, nzcv = i & 15;
        const std::string b = is_imm ? (std::to_string(imm_or_rm) + "ULL") : Xz(imm_or_rm);
        std::string s = "{ if (recomp_cond(c," + std::to_string(cond) + ")) { ";
        s += "uint64_t _a=" + Xz(rn) + ", _b=" + b + ", _r=" +
             std::string(op ? "_a-_b" : "_a+_b") + "; ";
        if (!sf) s += "_r &= 0xFFFFFFFFULL; ";
        s += "recomp_set_flags(c," + std::string(op ? "1" : "0") + ",_a,_b,_r," +
             (sf ? "1" : "0") + "); ";
        s += "} else { ";
        s += "c->n=" + std::to_string((nzcv >> 3) & 1) + "; ";
        s += "c->z=" + std::to_string((nzcv >> 2) & 1) + "; ";
        s += "c->c=" + std::to_string((nzcv >> 1) & 1) + "; ";
        s += "c->v=" + std::to_string(nzcv & 1) + "; } }";
        put(s);
        return true;
    }

    // Conditional select: CSEL/CSINC/CSINV/CSNEG.
    // Bit 11 must be clear: with it set this encoding is not a conditional
    // select at all, and decoding it as one silently invents an instruction.
    if ((i & 0x1FE00800) == 0x1A800000) {
        const u32 sf = i >> 31, op = (i >> 30) & 1, o2 = (i >> 10) & 1;
        const u32 rm = (i >> 16) & 31, cond = (i >> 12) & 15, rn = (i >> 5) & 31, rd = i & 31;
        if (rd != 31) {
            std::string a = Xz(rn), b = Xz(rm);
            std::string els = b;
            if (!op && o2) els = "(" + b + " + 1)";            // CSINC
            else if (op && !o2) els = "(~" + b + ")";           // CSINV
            else if (op && o2) els = "((uint64_t)(0 - " + b + "))"; // CSNEG
            std::string s = "{ uint64_t _r = recomp_cond(c," + std::to_string(cond) + ") ? " +
                            a + " : " + els + "; ";
            if (!sf) s += "_r &= 0xFFFFFFFFULL; ";
            s += "c->x[" + std::to_string(rd) + "] = _r; }";
            put(s);
            return true;
        }
    }

    // Load/store with unscaled 9-bit signed offset (LDUR/STUR) and the
    // pre/post-indexed immediate forms.
    // Again bit 26 (V) must be clear - the SIMD/FP form is handled below.
    // Bit 21 set with mode==0 is the ARMv8.1 LSE atomic group, not LDUR/STUR;
    // decoding those here would read a garbage imm9 out of the register field.
    if ((i & 0x3F000000) == 0x38000000 && ((i >> 24) & 1) == 0 && ((i >> 21) & 1) == 0) {
        const u32 size = i >> 30, opc = (i >> 22) & 3, mode = (i >> 10) & 3;
        const u32 rn = (i >> 5) & 31, rt = i & 31;
        s32 imm9 = (s32)((i >> 12) & 0x1FF);
        if (imm9 & 0x100) imm9 |= ~0x1FF;
        // mode 0 = LDUR/STUR, 1 = post-index, 3 = pre-index
        if ((mode == 0 || mode == 1 || mode == 3) && size <= 3) {
            const u32 bits = 8u << size;
            std::string s = "{ uint64_t _b=c->x[" + std::to_string(rn) + "]; int64_t _o=" +
                            std::to_string((long long)imm9) + "; ";
            const char* addr = (mode == 1) ? "_b" : "(_b+_o)";
            // opc selects the operation, and testing only its low bit gets this
            // wrong in both directions - the same mistake the unsigned-offset
            // handler already documents, never applied here. opc==2 (LDURSW /
            // LDURSB / LDURSH, sign-extending into a 64-bit destination) has
            // bit 0 clear and so was emitted as a STORE, turning tens of
            // thousands of ordinary signed loads into wild writes over live
            // guest memory; opc==3 loaded but never sign-extended.
            const char* signed_cast = size == 0   ? "(int8_t)"
                                      : size == 1 ? "(int16_t)"
                                                  : "(int32_t)";
            if (opc == 0) {
                s += "recomp_store" + std::to_string(bits) + "(c," + addr + "," + Xz(rt) + "); ";
            } else if (opc == 1) {
                if (rt != 31) {
                    s += "c->x[" + std::to_string(rt) + "]=recomp_load" + std::to_string(bits) +
                         "(c," + addr + "); ";
                }
            } else if (size == 3) {
                // opc>=2 with size==3 is PRFUM, a prefetch hint. It has no
                // architectural effect, so emitting nothing is exact - what it
                // must never do is store.
            } else if (rt != 31) {
                s += "c->x[" + std::to_string(rt) + "]=(uint64_t)(int64_t)" + signed_cast +
                     "recomp_load" + std::to_string(bits) + "(c," + addr + "); ";
                // opc==3 sign-extends into a 32-bit destination, so the result
                // is truncated back to W width after the extension.
                if (opc == 3) s += "c->x[" + std::to_string(rt) + "]&=0xFFFFFFFFULL; ";
            }
            if (mode == 1 || mode == 3) s += "c->x[" + std::to_string(rn) + "]=_b+_o; ";
            s += "}";
            put(s);
            return true;
        }
    }

    // Load/store exclusive and acquire/release.
    //
    // The non-exclusive acquire/release forms (LDAR/STLR, o2 set) are ordinary
    // loads and stores as far as this backend is concerned - suyu's memory
    // accessors are already atomic at these widths, and there is no weaker
    // ordering here to fence against - so they are translated directly.
    //
    // The genuinely exclusive forms (LDXR/LDAXR/STXR/STLXR, o2 clear) are not.
    // They used to become a plain load/store with STXR unconditionally
    // reporting success, which is exact only when nothing else can touch the
    // address. Under this backend real guest threads run concurrently, so an
    // always-succeeds STXR makes every compare-and-swap non-atomic: two
    // threads both "win" the same lock, the data it protects is then updated
    // from both, and the next thread to wait on it spins forever. Hand these
    // to the fallback engine, which owns the kernel's real exclusive monitor.
    if ((i & 0x3F000000) == 0x08000000) {
        const u32 size = i >> 30, o2 = (i >> 23) & 1, L = (i >> 22) & 1, o1 = (i >> 21) & 1;
        const u32 rt2 = (i >> 10) & 31, rn = (i >> 5) & 31, rt = i & 31;
        // Pair variants (o1 set) are left to the fallback; only the single
        // register forms are handled here.
        if (!o1 && rt2 == 31) {
            if (!o2) {
                snprintf(buf, sizeof buf,
                         "recomp_unhandled(c,0x%08xU,g_module_base+0x%llxULL); if(c->halted) return;",
                         i, (unsigned long long)pc);
                put(buf);
                return true;
            }
            const u32 bits = 8u << size;
            const std::string addr = "c->x[" + std::to_string(rn) + "]";
            if (L) {
                // LDAR
                if (rt != 31) {
                    put("c->x[" + std::to_string(rt) + "] = recomp_load" +
                        std::to_string(bits) + "(c," + addr + ");");
                }
            } else {
                // STLR - no status register.
                put("recomp_store" + std::to_string(bits) + "(c," + addr + "," + Xz(rt) + ");");
            }
            return true;
        }
    }

    // Data-processing 3-source: MADD/MSUB and the widening multiplies.
    if ((i & 0x1F000000) == 0x1B000000) {
        const u32 sf = i >> 31, op54 = (i >> 29) & 3, op31 = (i >> 21) & 7, o0 = (i >> 15) & 1;
        const u32 rm = (i >> 16) & 31, ra = (i >> 10) & 31, rn = (i >> 5) & 31, rd = i & 31;
        if (rd != 31 && op54 == 0) {
            std::string expr;
            if (op31 == 0) {
                // MADD / MSUB
                const std::string prod = "(" + Xz(rn) + " * " + Xz(rm) + ")";
                expr = Xz(ra) + (o0 ? " - " : " + ") + prod;
            } else if (op31 == 1 && !o0) {           // SMADDL
                expr = Xz(ra) + " + (uint64_t)((int64_t)(int32_t)" + Xz(rn) +
                       " * (int64_t)(int32_t)" + Xz(rm) + ")";
            } else if (op31 == 1 && o0) {            // SMSUBL
                expr = Xz(ra) + " - (uint64_t)((int64_t)(int32_t)" + Xz(rn) +
                       " * (int64_t)(int32_t)" + Xz(rm) + ")";
            } else if (op31 == 5 && !o0) {           // UMADDL
                expr = Xz(ra) + " + ((uint64_t)(uint32_t)" + Xz(rn) +
                       " * (uint64_t)(uint32_t)" + Xz(rm) + ")";
            } else if (op31 == 5 && o0) {            // UMSUBL
                expr = Xz(ra) + " - ((uint64_t)(uint32_t)" + Xz(rn) +
                       " * (uint64_t)(uint32_t)" + Xz(rm) + ")";
            } else if (op31 == 2) {                  // SMULH
                expr = "recomp_smulh(" + Xz(rn) + "," + Xz(rm) + ")";
            } else if (op31 == 6) {                  // UMULH
                expr = "recomp_umulh(" + Xz(rn) + "," + Xz(rm) + ")";
            }
            if (!expr.empty()) {
                std::string s = "{ uint64_t _r = " + expr + "; ";
                if (!sf && op31 == 0) s += "_r &= 0xFFFFFFFFULL; ";
                s += "c->x[" + std::to_string(rd) + "] = _r; }";
                put(s);
                return true;
            }
        }
    }

    // ADD/SUB extended register - the form used for pointer arithmetic with a
    // 32-bit index, so extremely common around array and struct accesses.
    if ((i & 0x1F200000) == 0x0B200000) {
        const u32 sf = i >> 31, op = (i >> 30) & 1, S = (i >> 29) & 1;
        const u32 rm = (i >> 16) & 31, option = (i >> 13) & 7, imm3 = (i >> 10) & 7;
        const u32 rn = (i >> 5) & 31, rd = i & 31;
        if (imm3 <= 4) {
            std::string ext;
            switch (option) {
            case 0: ext = "(uint64_t)(uint8_t)" + Xz(rm); break;           // UXTB
            case 1: ext = "(uint64_t)(uint16_t)" + Xz(rm); break;          // UXTH
            case 2: ext = "(uint64_t)(uint32_t)" + Xz(rm); break;          // UXTW
            case 3: ext = Xz(rm); break;                                    // UXTX/LSL
            case 4: ext = "(uint64_t)(int64_t)(int8_t)" + Xz(rm); break;   // SXTB
            case 5: ext = "(uint64_t)(int64_t)(int16_t)" + Xz(rm); break;  // SXTH
            case 6: ext = "(uint64_t)(int64_t)(int32_t)" + Xz(rm); break;  // SXTW
            case 7: ext = Xz(rm); break;                                    // SXTX
            default: break;
            }
            if (!ext.empty()) {
                if (imm3) ext = "((" + ext + ") << " + std::to_string(imm3) + ")";
                // Rn is SP here, not the zero register, and so is Rd unless the
                // flag-setting form is used - where it really is the zero
                // register, because that is CMP.
                std::string s = "{ uint64_t _a=" + Xsp(rn) + ", _b=" + ext + "; uint64_t _r=" +
                                std::string(op ? "_a-_b" : "_a+_b") + "; ";
                if (!sf) s += "_r &= 0xFFFFFFFFULL; ";
                if (rd != 31 || !S) s += "c->x[" + std::to_string(rd) + "]=_r; ";
                if (S) s += "recomp_set_flags(c," + std::string(op ? "1" : "0") +
                            ",_a,_b,_r," + (sf ? "1" : "0") + "); ";
                s += "}";
                put(s);
                return true;
            }
        }
    }

    // Load/store with a register offset (the [base, Xm{, extend}] form).
    // Bit 26 (V) clear: general registers only, SIMD/FP form handled below.
    if ((i & 0x3F200C00) == 0x38200800) {
        const u32 size = i >> 30, opc = (i >> 22) & 3;
        const u32 rm = (i >> 16) & 31, option = (i >> 13) & 7, S = (i >> 12) & 1;
        const u32 rn = (i >> 5) & 31, rt = i & 31;
        // opc 00 stores, 01 loads zero-extended, 10 loads sign-extended to 64
        // bits and 11 sign-extended to 32. The signed forms are common - an
        // indexed read of an int32 array compiles to LDRSW with a register
        // offset - so leaving them out put thousands of real loads on the
        // fallback.
        if (size <= 3 && !(opc >= 2 && size == 3)) {
            const u32 bits = 8u << size;
            std::string idx;
            switch (option) {
            case 2: idx = "(uint64_t)(uint32_t)" + Xz(rm); break;          // UXTW
            case 3: idx = Xz(rm); break;                                    // LSL
            case 6: idx = "(uint64_t)(int64_t)(int32_t)" + Xz(rm); break;  // SXTW
            case 7: idx = Xz(rm); break;                                    // SXTX
            default: break;
            }
            if (!idx.empty()) {
                if (S && size) idx = "((" + idx + ") << " + std::to_string(size) + ")";
                const std::string addr = "(c->x[" + std::to_string(rn) + "] + " + idx + ")";
                if (opc == 0) {
                    put("recomp_store" + std::to_string(bits) + "(c," + addr + "," + Xz(rt) + ");");
                } else if (opc == 1) {
                    if (rt != 31) {
                        put("c->x[" + std::to_string(rt) + "] = recomp_load" +
                            std::to_string(bits) + "(c," + addr + ");");
                    }
                } else if (rt != 31) {
                    const char* st = size == 0 ? "int8_t" : size == 1 ? "int16_t" : "int32_t";
                    std::string s = "{ uint64_t _r = (uint64_t)(int64_t)(" + std::string(st) +
                                    ")recomp_load" + std::to_string(bits) + "(c," + addr + "); ";
                    if (opc == 3) s += "_r &= 0xFFFFFFFFULL; ";   // 32-bit destination
                    s += "c->x[" + std::to_string(rt) + "] = _r; }";
                    put(s);
                }
                return true;
            }
        }
    }

    // FP <-> integer conversions and FMOV between register files. These share
    // bit 21 with the FP arithmetic forms and are distinguished by bits 15..10
    // being zero, so they must be decoded ahead of the arithmetic/compare block
    // below or every one of them is mis-decoded as FCMP.
    if ((i & 0x5F200000) == 0x1E200000 && ((i >> 21) & 1) && ((i >> 10) & 0x3F) == 0) {
        const u32 sf = i >> 31, ftype = (i >> 22) & 3;
        const u32 rmode = (i >> 19) & 3, opcode = (i >> 16) & 7;
        const u32 rn = (i >> 5) & 31, rd = i & 31;
        if (ftype == 0 || ftype == 1) {
            const bool dbl = (ftype == 1);
            const char* ct = dbl ? "double" : "float";
            const int fsz = dbl ? 8 : 4;
            const std::string zero_d = "c->vreg[" + std::to_string(rd) + "][0]=0; c->vreg[" +
                                       std::to_string(rd) + "][1]=0; ";

            // SCVTF / UCVTF: integer register -> FP register.
            if (rmode == 0 && (opcode == 2 || opcode == 3)) {
                const std::string src = sf ? (opcode == 2 ? "(int64_t)" + Xz(rn)
                                                          : "(uint64_t)" + Xz(rn))
                                           : (opcode == 2 ? "(int32_t)" + Xz(rn)
                                                          : "(uint32_t)" + Xz(rn));
                put("{ " + std::string(ct) + " _r = (" + ct + ")(" + src + "); " + zero_d +
                    "memcpy(&c->vreg[" + std::to_string(rd) + "][0],&_r," +
                    std::to_string(fsz) + "); }");
                return true;
            }

            // FCVTZS / FCVTZU: FP register -> integer register, round toward zero.
            if (rmode == 3 && (opcode == 0 || opcode == 1) && rd != 31) {
                const char* it = sf ? (opcode == 0 ? "int64_t" : "uint64_t")
                                    : (opcode == 0 ? "int32_t" : "uint32_t");
                std::string s = "{ " + std::string(ct) + " _a; memcpy(&_a,&c->vreg[" +
                                std::to_string(rn) + "][0]," + std::to_string(fsz) + "); ";
                // FCVTZS/FCVTZU saturate: NaN gives 0, and anything outside the
                // destination's range clamps to that range's min or max. A bare
                // C cast is undefined for exactly those inputs, and on x86 it
                // compiles to cvttss2si, which answers "integer indefinite"
                // (INT_MIN / LLONG_MIN) for NaN, +inf and -inf alike. Float-heavy
                // game code converts out-of-range values constantly - clamped
                // indices, hashes, fixed-point - so the wrong answer here is a
                // steady drip of corruption rather than an immediate fault.
                const bool is_signed = (opcode == 0);
                const char* lo_bound = sf ? (is_signed ? "-9223372036854775808.0" : "0.0")
                                          : (is_signed ? "-2147483648.0" : "0.0");
                const char* hi_bound = sf ? (is_signed ? "9223372036854775807.0"
                                                       : "18446744073709551615.0")
                                          : (is_signed ? "2147483647.0" : "4294967295.0");
                const char* sat_lo = sf ? (is_signed ? "0x8000000000000000ULL" : "0ULL")
                                        : (is_signed ? "0xFFFFFFFF80000000ULL" : "0ULL");
                const char* sat_hi = sf ? (is_signed ? "0x7FFFFFFFFFFFFFFFULL"
                                                     : "0xFFFFFFFFFFFFFFFFULL")
                                        : (is_signed ? "0x7FFFFFFFULL" : "0xFFFFFFFFULL");
                s += "uint64_t _r; if (_a != _a) _r = 0ULL; ";
                s += std::string("else if (!(_a > (") + ct + ")" + lo_bound + ")) _r = " + sat_lo + "; ";
                s += std::string("else if (!(_a < (") + ct + ")" + hi_bound + ")) _r = " + sat_hi + "; ";
                s += "else _r = (uint64_t)(" + std::string(it) + ")_a; ";
                if (!sf) s += "_r &= 0xFFFFFFFFULL; ";
                s += "c->x[" + std::to_string(rd) + "] = _r; }";
                put(s);
                return true;
            }

            // FMOV between a general register and an FP register.
            if (rmode == 0 && (opcode == 6 || opcode == 7)) {
                if (opcode == 7) {           // GPR -> FP
                    put("{ " + zero_d + "memcpy(&c->vreg[" + std::to_string(rd) + "][0],&" +
                        (rn == 31 ? std::string("(uint64_t){0}") : "c->x[" + std::to_string(rn) + "]") +
                        "," + std::to_string(fsz) + "); }");
                    return true;
                }
                if (rd != 31) {              // FP -> GPR
                    put("{ uint64_t _r=0; memcpy(&_r,&c->vreg[" + std::to_string(rn) + "][0]," +
                        std::to_string(fsz) + "); c->x[" + std::to_string(rd) + "]=_r; }");
                    return true;
                }
            }
        }
    }

    // FCSEL - the floating-point conditional select. Shares the scalar FP
    // encoding but is picked out by bits 11..10 being 11, so it is matched
    // before the arithmetic and compare forms below.
    if ((i & 0x5F200C00) == 0x1E200C00) {
        const u32 ftype = (i >> 22) & 3;
        const u32 rm = (i >> 16) & 31, cond = (i >> 12) & 15;
        const u32 rn = (i >> 5) & 31, rd = i & 31;
        if (ftype == 0 || ftype == 1) {
            const int fsz = (ftype == 1) ? 8 : 4;
            // Selecting whole register halves rather than reinterpreting the
            // value keeps this exact for NaN payloads too.
            put("{ uint64_t _r = recomp_cond(c," + std::to_string(cond) + ") ? c->vreg[" +
                std::to_string(rn) + "][0] : c->vreg[" + std::to_string(rm) + "][0]; " +
                (fsz == 4 ? "_r &= 0xFFFFFFFFULL; " : "") + "c->vreg[" + std::to_string(rd) +
                "][0]=_r; c->vreg[" + std::to_string(rd) + "][1]=0; }");
            return true;
        }
    }

    // Data-processing (1 source): RBIT, REV16, REV32, REV, CLZ and CLS.
    // The mask must stop at bit 16: bits 15..10 are the opcode being switched
    // on below, so including them would pin this to RBIT alone.
    if ((i & 0x7FFF0000) == 0x5AC00000) {
        const u32 sf = i >> 31, opcode = (i >> 10) & 0x3F;
        const u32 rn = (i >> 5) & 31, rd = i & 31;
        const u32 width = sf ? 64 : 32;
        if (rd != 31) {
            const std::string w = std::to_string(width);
            const std::string src =
                sf ? Xz(rn) : ("(" + Xz(rn) + " & 0xFFFFFFFFULL)");
            std::string s;
            switch (opcode) {
            case 0:   // RBIT - reverse every bit
                s = "{ uint64_t _v=" + src + ", _r=0; for(int _i=0;_i<" + w +
                    ";_i++){ _r=(_r<<1)|((_v>>_i)&1ULL); } c->x[" + std::to_string(rd) + "]=_r; }";
                break;
            case 1:   // REV16 - reverse bytes within each halfword
                s = "{ uint64_t _v=" + src + ", _r=0; for(int _i=0;_i<" + w +
                    "/8;_i++){ int _p=(_i^1)*8; _r|=((_v>>(_i*8))&0xFFULL)<<_p; } c->x[" +
                    std::to_string(rd) + "]=_r; }";
                break;
            case 2:   // REV32 on 64-bit, REV on 32-bit: bytes within each word
                if (!sf) {
                    s = "{ uint64_t _v=" + src + ", _r=0; for(int _i=0;_i<4;_i++){ "
                        "_r|=((_v>>(_i*8))&0xFFULL)<<((3-_i)*8); } c->x[" +
                        std::to_string(rd) + "]=_r; }";
                } else {
                    s = "{ uint64_t _v=" + src + ", _r=0; for(int _i=0;_i<8;_i++){ "
                        "int _p=((_i&4)|(3-(_i&3)))*8; _r|=((_v>>(_i*8))&0xFFULL)<<_p; } c->x[" +
                        std::to_string(rd) + "]=_r; }";
                }
                break;
            case 3:   // REV (64-bit only) - reverse all bytes
                if (sf) {
                    s = "{ uint64_t _v=" + src + ", _r=0; for(int _i=0;_i<8;_i++){ "
                        "_r|=((_v>>(_i*8))&0xFFULL)<<((7-_i)*8); } c->x[" +
                        std::to_string(rd) + "]=_r; }";
                }
                break;
            case 4:   // CLZ
                s = "{ uint64_t _v=" + src + "; int _n=0; while(_n<" + w + " && !((_v>>(" + w +
                    "-1-_n))&1ULL)) _n++; c->x[" + std::to_string(rd) + "]=(uint64_t)_n; }";
                break;
            case 5:   // CLS - leading sign bits, not counting the sign itself
                s = "{ uint64_t _v=" + src + "; int _n=0; while(_n<" + w +
                    "-1 && (((_v>>(" + w + "-1-_n))&1ULL)==((_v>>(" + w +
                    "-2-_n))&1ULL))) _n++; c->x[" + std::to_string(rd) + "]=(uint64_t)_n; }";
                break;
            default: break;
            }
            if (!s.empty()) { put(s); return true; }
        }
    }

    // MRS/MSR against the small set of system registers a user-mode program
    // actually touches. Anything else stays on the fallback rather than being
    // silently invented.
    if ((i & 0xFFF00000) == 0xD5300000 || (i & 0xFFF00000) == 0xD5100000) {
        const bool is_read = ((i >> 21) & 1) != 0;   // MRS reads, MSR writes
        const u32 sysreg = (i >> 5) & 0x7FFF;
        const u32 rt = i & 31;
        // TPIDR_EL0 (thread pointer) and TPIDRRO_EL0 are the ones that matter
        // for ordinary code; both live in the context already. They are kept
        // in *separate* fields deliberately: TPIDR_EL0 is the guest's own
        // thread pointer, while TPIDRRO_EL0 is written by the kernel and holds
        // the thread-local region whose first bytes are the IPC message
        // buffer. Folding them together corrupts both.
        constexpr u32 kTpidrEl0 = 0x5E82;
        constexpr u32 kTpidrroEl0 = 0x5E83;
        if (sysreg == kTpidrEl0) {
            if (is_read) {
                if (rt != 31) put("c->x[" + std::to_string(rt) + "] = c->tpidr_el0;");
            } else {
                put("c->tpidr_el0 = " + Xz(rt) + ";");
            }
            return true;
        }
        if (sysreg == kTpidrroEl0) {
            if (is_read) {
                if (rt != 31) put("c->x[" + std::to_string(rt) + "] = c->tpidrro_el0;");
            } else {
                // Read-only at EL0; a write traps on hardware. Swallow it
                // rather than letting it destroy the kernel's TLS pointer.
                put("(void)" + Xz(rt) + ";");
            }
            return true;
        }
    }

    // Scalar floating point. Only single (ftype 00) and double (ftype 01) are
    // handled; half-precision and the SIMD vector forms still fall through.
    // Values move through memcpy rather than type punning so this stays
    // strictly conforming C.
    if ((i & 0x5F000000) == 0x1E000000 && ((i >> 21) & 1)) {
        const u32 ftype = (i >> 22) & 3;
        const u32 rm = (i >> 16) & 31, rn = (i >> 5) & 31, rd = i & 31;
        if (ftype == 0 || ftype == 1) {
            const bool dbl = (ftype == 1);
            const char* ct = dbl ? "double" : "float";
            const int sz = dbl ? 8 : 4;
            const std::string ld_n = std::string("{ ") + ct + " _a,_b,_r; memcpy(&_a,&c->vreg[" +
                                     std::to_string(rn) + "][0]," + std::to_string(sz) + "); ";
            const std::string ld_m = std::string("memcpy(&_b,&c->vreg[") + std::to_string(rm) +
                                     "][0]," + std::to_string(sz) + "); ";
            const std::string st_d = std::string("c->vreg[") + std::to_string(rd) +
                                     "][0]=0; c->vreg[" + std::to_string(rd) +
                                     "][1]=0; memcpy(&c->vreg[" + std::to_string(rd) + "][0],&_r," +
                                     std::to_string(sz) + "); }";

            // FMOV (immediate). This must be decoded before FCMP below: it
            // also has bits 11..10 clear, so an FMOV whose imm8 and Rd happen
            // to line up would otherwise be read as a compare and silently
            // clobber the flags instead of loading a constant.
            if (((i >> 10) & 7) == 4 && ((i >> 5) & 0x1F) == 0) {
                const u32 imm8 = (i >> 13) & 0xFF;
                // VFPExpandImm: sign, then exponent as NOT(b6) followed by b6
                // repeated, then imm8<5:4>, then imm8<3:0> as the top of the
                // mantissa. Built at double width and narrowed if needed.
                const u32 sgn = (imm8 >> 7) & 1;
                const u32 b6 = (imm8 >> 6) & 1;
                const u64 e11 = ((u64)(b6 ^ 1) << 10) | (b6 ? (0xFFULL << 2) : 0ULL) |
                                ((imm8 >> 4) & 3);
                const u64 dbits = ((u64)sgn << 63) | (e11 << 52) | ((u64)(imm8 & 0xF) << 48);
                char hb[64];
                if (dbl) {
                    snprintf(hb, sizeof hb, "0x%llxULL", (unsigned long long)dbits);
                } else {
                    double dv; memcpy(&dv, &dbits, 8);
                    const float fv = (float)dv;
                    u32 fbits; memcpy(&fbits, &fv, 4);
                    snprintf(hb, sizeof hb, "0x%xULL", fbits);
                }
                put("c->vreg[" + std::to_string(rd) + "][0]=" + hb + "; c->vreg[" +
                    std::to_string(rd) + "][1]=0;");
                return true;
            }

            // Two-source: FMUL/FDIV/FADD/FSUB (opcode in bits 15..12).
            if (((i >> 10) & 3) == 2) {
                const u32 opcode = (i >> 12) & 15;
                const char* op = nullptr;
                switch (opcode) {
                case 0: op = "*"; break;
                case 1: op = "/"; break;
                case 2: op = "+"; break;
                case 3: op = "-"; break;
                default: break;
                }
                if (op) {
                    put(ld_n + ld_m + "_r = _a " + op + " _b; " + st_d);
                    return true;
                }
                // FMAX/FMIN propagate a NaN operand; the NM forms return the
                // other operand instead, which is exactly what fmax/fmin do.
                std::string expr2;
                switch (opcode) {
                case 4: expr2 = "(_a!=_a||_b!=_b) ? (_a+_b) : (_a>_b?_a:_b)"; break;  // FMAX
                case 5: expr2 = "(_a!=_a||_b!=_b) ? (_a+_b) : (_a<_b?_a:_b)"; break;  // FMIN
                case 6: expr2 = dbl ? "fmax(_a,_b)" : "fmaxf(_a,_b)"; break;          // FMAXNM
                case 7: expr2 = dbl ? "fmin(_a,_b)" : "fminf(_a,_b)"; break;          // FMINNM
                case 8: expr2 = "-(_a*_b)"; break;                                    // FNMUL
                default: break;
                }
                if (!expr2.empty()) {
                    put(ld_n + ld_m + "_r = " + expr2 + "; " + st_d);
                    return true;
                }
            }

            // FCVT between precisions. It shares the one-source encoding but
            // its source and destination widths differ, so it cannot use the
            // shared load/store fragments below, which assume both are ftype.
            // Opcode 0001xx, where the low two bits name the destination:
            // 00 single, 01 double, 11 half. Half stays on the fallback.
            if (((i >> 10) & 0x1F) == 0x10 && (((i >> 15) & 0x3C) == 0x04)) {
                const u32 dst = (i >> 15) & 3;
                if (ftype == 0 && dst == 1) {           // single -> double
                    put("{ float _s; double _d; memcpy(&_s,&c->vreg[" + std::to_string(rn) +
                        "][0],4); _d=(double)_s; c->vreg[" + std::to_string(rd) +
                        "][1]=0; memcpy(&c->vreg[" + std::to_string(rd) + "][0],&_d,8); }");
                    return true;
                }
                if (ftype == 1 && dst == 0) {           // double -> single
                    put("{ double _d; float _s; memcpy(&_d,&c->vreg[" + std::to_string(rn) +
                        "][0],8); _s=(float)_d; c->vreg[" + std::to_string(rd) +
                        "][0]=0; c->vreg[" + std::to_string(rd) +
                        "][1]=0; memcpy(&c->vreg[" + std::to_string(rd) + "][0],&_s,4); }");
                    return true;
                }
            }

            // One-source: FMOV/FABS/FNEG/FSQRT (opcode in bits 20..15 low bits).
            if (((i >> 10) & 0x1F) == 0x10) {
                const u32 opcode = (i >> 15) & 0x3F;
                std::string expr;
                switch (opcode) {
                case 0: expr = "_a"; break;                             // FMOV
                case 1: expr = dbl ? "fabs(_a)" : "fabsf(_a)"; break;   // FABS
                case 2: expr = "-_a"; break;                            // FNEG
                case 3: expr = dbl ? "sqrt(_a)" : "sqrtf(_a)"; break;   // FSQRT
                default: break;
                }
                if (!expr.empty()) {
                    put(ld_n + "(void)_b; _r = " + expr + "; " + st_d);
                    return true;
                }
            }

            // FCMP / FCMPE, including the compare-against-zero forms. The
            // low five bits are opcode2: bit 3 selects the #0.0 variant (Rm is
            // then not a register at all) and bit 4 selects the signalling
            // form, which differs only in how it reports NaNs and so produces
            // the same flags here. Requiring all five to be clear, as before,
            // matched only a quarter of the compares in real code.
            if (((i >> 10) & 0xF) == 8 && ((i >> 14) & 3) == 0 && (i & 7) == 0) {
                const bool cmp_zero = ((i >> 3) & 1) != 0;
                std::string s = ld_n;
                if (cmp_zero) {
                    s += std::string("_b = (") + ct + ")0; ";
                } else {
                    s += ld_m;
                }
                // Unordered (either NaN) sets C and V per the architecture.
                s += "if (_a != _a || _b != _b) { c->n=0; c->z=0; c->c=1; c->v=1; } ";
                s += "else { c->n = (_a < _b); c->z = (_a == _b); "
                     "c->c = (_a >= _b); c->v = 0; } ";
                s += "(void)_r; }";
                put(s);
                return true;
            }
        }
    }

    // Data-processing 2-source: UDIV, SDIV, and variable-shift forms LSLV/LSRV/ASRV/RORV.
    // Encoding: sf 0 0 1 1 0 1 0 1 Rm opcode Rn Rd
    if ((i & 0x5FE00000) == 0x1AC00000) {
        const u32 sf = i >> 31;
        const u32 rm = (i >> 16) & 31, opcode = (i >> 10) & 63;
        const u32 rn = (i >> 5) & 31, rd = i & 31;
        if (rd != 31) {
            // Register 31 reads as XZR in this group, not SP.
            const std::string xn = Xz(rn);
            const std::string xm = Xz(rm);
            std::string s;
            switch (opcode) {
            case 2:  // UDIV - divide by 0 yields 0 per ARM spec
                if (sf) s = "{ uint64_t _r=" + xm + "?" + xn + "/" + xm + ":0; c->x[" + std::to_string(rd) + "]=_r; }";
                else    s = "{ uint32_t _a=(uint32_t)" + xn + ",_b=(uint32_t)" + xm + "; c->x[" + std::to_string(rd) + "]=(uint64_t)(_b?_a/_b:0); }";
                break;
            case 3:  // SDIV
                if (sf) s = "{ int64_t _a=(int64_t)" + xn + ",_b=(int64_t)" + xm + "; c->x[" + std::to_string(rd) + "]=(uint64_t)(_b?_a/_b:0); }";
                else    s = "{ int32_t _a=(int32_t)(uint32_t)" + xn + ",_b=(int32_t)(uint32_t)" + xm + "; c->x[" + std::to_string(rd) + "]=(uint64_t)(uint32_t)(_b?_a/_b:0); }";
                break;
            case 8:  // LSLV
                if (sf) s = "{ c->x[" + std::to_string(rd) + "]=" + xn + "<<(" + xm + "&63); }";
                else    s = "{ c->x[" + std::to_string(rd) + "]=(uint64_t)(uint32_t)((uint32_t)" + xn + "<<(" + xm + "&31)); }";
                break;
            case 9:  // LSRV
                if (sf) s = "{ c->x[" + std::to_string(rd) + "]=" + xn + ">>(" + xm + "&63); }";
                else    s = "{ c->x[" + std::to_string(rd) + "]=(uint64_t)((uint32_t)" + xn + ">>(" + xm + "&31)); }";
                break;
            case 10: // ASRV
                if (sf) s = "{ c->x[" + std::to_string(rd) + "]=(uint64_t)((int64_t)" + xn + ">>(" + xm + "&63)); }";
                else    s = "{ c->x[" + std::to_string(rd) + "]=(uint64_t)(uint32_t)((int32_t)(uint32_t)" + xn + ">>(" + xm + "&31)); }";
                break;
            case 11: // RORV
                if (sf) s = "{ uint64_t _a=" + xn + ",_s=" + xm + "&63; c->x[" + std::to_string(rd) + "]=_s?(_a>>_s)|(_a<<(64-_s)):_a; }";
                else    s = "{ uint32_t _a=(uint32_t)" + xn + ",_s=(uint32_t)" + xm + "&31; c->x[" + std::to_string(rd) + "]=(uint64_t)(uint32_t)(_s?(_a>>_s)|(_a<<(32-_s)):_a); }";
                break;
            }
            if (!s.empty()) { put(s); return true; }
        }
    }

    // Advanced SIMD two-register misc, floating-point compare against zero:
    // FCMGT, FCMGE, FCMEQ, FCMLE and FCMLT. A true lane is all-ones, which is
    // what the following select/AND normally consumes. Only the bit-23-clear
    // group is decoded; the rest stays on the fallback.
    {
        const bool vec_misc = (i & 0x9F3E0C00) == 0x0E200800;
        // Bit 29 is U and must stay out of the mask, or only the U=0 half of
        // each pair (FCMEQ but not FCMLE, FCMGT but not FCMGE) would match.
        const bool scl_misc = (i & 0xDF3E0C00) == 0x5E200800;
        if (vec_misc || scl_misc) {
            const u32 Q = (i >> 30) & 1, U = (i >> 29) & 1;
            const u32 opcode = (i >> 12) & 0x1F;
            const bool dbl = ((i >> 22) & 1) != 0;
            const u32 rn = (i >> 5) & 31, rd = i & 31;
            const char* cmp = nullptr;
            if (opcode == 0x0C) cmp = U ? ">=" : ">";      // FCMGE / FCMGT
            else if (opcode == 0x0D) cmp = U ? "<=" : "=="; // FCMLE / FCMEQ
            else if (opcode == 0x0E && !U) cmp = "<";       // FCMLT
            // SCVTF / UCVTF: convert the integer in each lane to a float of the
            // same width. This is the vector counterpart of the general-register
            // conversion handled further up - the operand is a lane here, not a
            // general register.
            if (opcode == 0x1D) {
                const char* ct = dbl ? "double" : "float";
                const int fsz = dbl ? 8 : 4;
                const int bytes = scl_misc ? fsz : (Q ? 16 : 8);
                const int lanes = bytes / fsz;
                const std::string ity = std::string(U ? "uint" : "int") +
                                        std::to_string(fsz * 8) + "_t";
                std::string s = "{ " + ity + " _a[" + std::to_string(lanes) + "]; " +
                                std::string(ct) + " _r[" + std::to_string(lanes) + "]; ";
                s += "memcpy(_a,c->vreg[" + std::to_string(rn) + "]," + std::to_string(bytes) + "); ";
                s += "for(int _i=0;_i<" + std::to_string(lanes) + ";_i++) _r[_i]=(" +
                     std::string(ct) + ")_a[_i]; ";
                s += "c->vreg[" + std::to_string(rd) + "][0]=0; c->vreg[" + std::to_string(rd) +
                     "][1]=0; ";
                s += "memcpy(c->vreg[" + std::to_string(rd) + "],_r," + std::to_string(bytes) + "); }";
                put(s);
                return true;
            }
            if (cmp) {
                const char* ct = dbl ? "double" : "float";
                const int fsz = dbl ? 8 : 4;
                // A scalar form touches one lane; a vector form covers the
                // whole selected width.
                const int bytes = scl_misc ? fsz : (Q ? 16 : 8);
                const int lanes = bytes / fsz;
                const std::string uty = "uint" + std::to_string(fsz * 8) + "_t";
                std::string s = "{ " + std::string(ct) + " _a[" + std::to_string(lanes) + "]; " +
                                uty + " _r[" + std::to_string(lanes) + "]; ";
                s += "memcpy(_a,c->vreg[" + std::to_string(rn) + "]," + std::to_string(bytes) + "); ";
                s += "for(int _i=0;_i<" + std::to_string(lanes) + ";_i++) _r[_i]=(_a[_i]" + cmp +
                     "(" + ct + ")0) ? (" + uty + ")~(" + uty + ")0 : (" + uty + ")0; ";
                s += "c->vreg[" + std::to_string(rd) + "][0]=0; c->vreg[" + std::to_string(rd) +
                     "][1]=0; ";
                s += "memcpy(c->vreg[" + std::to_string(rd) + "],_r," + std::to_string(bytes) + "); }";
                put(s);
                return true;
            }
        }
    }

    // FMUL by indexed element. Rm is only four bits here, extended by M, and
    // the lane index is split across H and L - reading Rm as the usual five
    // bits would silently address the wrong register.
    {
        // Bit 29 (U) selects FMULX, a different operation - keep it out of FMUL.
        const bool vec_idx = (i & 0xBF00F400) == 0x0F009000;
        const bool scl_idx = (i & 0xFF00F400) == 0x5F009000;
        if ((vec_idx || scl_idx) && ((i >> 23) & 1) == 1) {
            const u32 Q = (i >> 30) & 1;
            const bool dbl = ((i >> 22) & 1) != 0;
            const u32 rn = (i >> 5) & 31, rd = i & 31;
            const u32 rm = ((i >> 16) & 15) | (((i >> 20) & 1) << 4);
            const u32 H = (i >> 11) & 1, L = (i >> 21) & 1;
            const u32 index = dbl ? H : ((H << 1) | L);
            const char* ct = dbl ? "double" : "float";
            const int fsz = dbl ? 8 : 4;
            const int bytes = scl_idx ? fsz : (Q ? 16 : 8);
            const int lanes = bytes / fsz;
            if (!(dbl && L)) {   // L must be zero for the 64-bit form
                std::string s = "{ " + std::string(ct) + " _a[" + std::to_string(lanes) +
                                "],_r[" + std::to_string(lanes) + "],_m; ";
                s += "memcpy(_a,c->vreg[" + std::to_string(rn) + "]," + std::to_string(bytes) + "); ";
                s += "memcpy(&_m,(const uint8_t*)c->vreg[" + std::to_string(rm) + "]+" +
                     std::to_string(index * fsz) + "," + std::to_string(fsz) + "); ";
                s += "for(int _i=0;_i<" + std::to_string(lanes) + ";_i++) _r[_i]=_a[_i]*_m; ";
                s += "c->vreg[" + std::to_string(rd) + "][0]=0; c->vreg[" + std::to_string(rd) +
                     "][1]=0; ";
                s += "memcpy(c->vreg[" + std::to_string(rd) + "],_r," + std::to_string(bytes) + "); }";
                put(s);
                return true;
            }
        }
    }

    // Advanced SIMD copy: DUP, INS, SMOV and UMOV. These move single lanes
    // between vector registers and the general registers, which is how any
    // scalar value gets into or out of vector code - so they turn up
    // constantly. imm5 encodes both the element width and the lane index:
    // the position of its lowest set bit gives the width, and the bits above
    // it give the index.
    {
        const bool vector_copy = (i & 0x9FE08400) == 0x0E000400;
        const bool scalar_copy = (i & 0xFFE08400) == 0x5E000400;
        if (vector_copy || scalar_copy) {
            const u32 Q = (i >> 30) & 1, op = (i >> 29) & 1;
            const u32 imm5 = (i >> 16) & 0x1F, imm4 = (i >> 11) & 0xF;
            const u32 rn = (i >> 5) & 31, rd = i & 31;
            int size = -1;
            u32 index = 0;
            if (imm5 & 1)      { size = 0; index = imm5 >> 1; }
            else if (imm5 & 2) { size = 1; index = imm5 >> 2; }
            else if (imm5 & 4) { size = 2; index = imm5 >> 3; }
            else if (imm5 & 8) { size = 3; index = imm5 >> 4; }
            if (size >= 0) {
                const int esz = 1 << size;
                const std::string sn = std::to_string(rn), sd = std::to_string(rd);
                const std::string off = std::to_string(index * esz);
                // Read the source lane into _e as a byte copy; this avoids any
                // assumption about how the 128-bit register is split in two.
                const std::string read_lane =
                    "uint8_t _s[16]; memcpy(_s,c->vreg[" + sn + "],16); uint64_t _e=0; "
                    "memcpy(&_e,_s+" + off + "," + std::to_string(esz) + "); ";

                if (scalar_copy && !op && imm4 == 0) {
                    // DUP (scalar): the named lane alone, rest of the register zeroed.
                    put("{ " + read_lane + "c->vreg[" + sd + "][0]=_e; c->vreg[" + sd +
                        "][1]=0; }");
                    return true;
                }
                if (vector_copy && !op && (imm4 == 0 || imm4 == 1)) {
                    // DUP (element) or DUP (general): every lane takes the value.
                    const int lanes = (Q ? 16 : 8) / esz;
                    std::string s = "{ ";
                    s += (imm4 == 0) ? read_lane
                                     : ("uint64_t _e=" + Xz(rn) + "; ");
                    s += "uint8_t _d[16]; memset(_d,0,16); ";
                    s += "for(int _i=0;_i<" + std::to_string(lanes) + ";_i++) memcpy(_d+_i*" +
                         std::to_string(esz) + ",&_e," + std::to_string(esz) + "); ";
                    s += "memcpy(c->vreg[" + sd + "],_d,16); }";
                    put(s);
                    return true;
                }
                if (vector_copy && !op && (imm4 == 5 || imm4 == 7) && rd != 31) {
                    // SMOV / UMOV: one lane out into a general register.
                    std::string s = "{ " + read_lane;
                    if (imm4 == 5) {
                        // Sign-extend from the element width.
                        const char* st = size == 0 ? "int8_t"
                                       : size == 1 ? "int16_t"
                                                   : "int32_t";
                        s += "uint64_t _r=(uint64_t)(int64_t)(" + std::string(st) + ")_e; ";
                        if (!Q) s += "_r &= 0xFFFFFFFFULL; ";
                        s += "c->x[" + sd + "]=_r; }";
                    } else {
                        s += "c->x[" + sd + "]=_e; }";
                    }
                    put(s);
                    return true;
                }
                if (vector_copy && !op && imm4 == 3) {
                    // INS (general): overwrite one lane, leave the others alone.
                    put("{ uint8_t _d[16]; memcpy(_d,c->vreg[" + sd + "],16); uint64_t _e=" +
                        Xz(rn) + "; memcpy(_d+" + off + ",&_e," + std::to_string(esz) +
                        "); memcpy(c->vreg[" + sd + "],_d,16); }");
                    return true;
                }
                if (vector_copy && op) {
                    // INS (element): imm4 holds the source lane, above the bits
                    // the element width occupies.
                    const u32 src_index = imm4 >> size;
                    put("{ uint8_t _s[16],_d[16]; memcpy(_s,c->vreg[" + sn +
                        "],16); memcpy(_d,c->vreg[" + sd + "],16); memcpy(_d+" + off + ",_s+" +
                        std::to_string(src_index * esz) + "," + std::to_string(esz) +
                        "); memcpy(c->vreg[" + sd + "],_d,16); }");
                    return true;
                }
            }
        }
    }

    // Advanced SIMD modified immediate - MOVI and MVNI. The immediate is
    // scattered across two fields (abc at 18..16, defgh at 9..5) and cmode
    // decides how those eight bits expand, so it has to be rebuilt rather
    // than read out directly.
    if ((i & 0x9FF80400) == 0x0F000400) {
        const u32 Q = (i >> 30) & 1, op = (i >> 29) & 1;
        const u32 cmode = (i >> 12) & 0xF;
        const u32 rd = i & 31;
        const u32 imm8 = (((i >> 16) & 7) << 5) | ((i >> 5) & 0x1F);
        u64 lo = 0;
        bool ok = false;
        if (cmode == 0xE) {
            if (!op) {
                // MOVI Vd.8B/16B, #imm8 - the byte repeated across the vector.
                lo = 0x0101010101010101ULL * (u64)imm8;
                ok = true;
            } else {
                // MOVI Vd.2D / Dd, #imm64 - each bit of imm8 expands to a
                // whole byte of 0x00 or 0xFF.
                for (u32 b = 0; b < 8; ++b) {
                    if ((imm8 >> b) & 1) lo |= 0xFFULL << (b * 8);
                }
                ok = true;
            }
        }
        if (ok) {
            char hb[48];
            snprintf(hb, sizeof hb, "0x%llxULL", (unsigned long long)lo);
            // op=1 with cmode=1110 is a 64-bit value: it fills the upper half
            // only when Q selects the full 128-bit register. The 8-bit form
            // replicates across whichever width Q selects.
            const std::string hi = Q ? std::string(hb) : std::string("0");
            put("c->vreg[" + std::to_string(rd) + "][0]=" + hb + "; c->vreg[" +
                std::to_string(rd) + "][1]=" + hi + ";");
            return true;
        }
    }

    // Advanced SIMD three-register same (0x0E/0x4E/0x6E group).
    // Bit pattern: Q U 0 1 1 1 0 size 1 Rm opcode 1 Rn Rd
    // Fixed bits: [28:24]=01110, bit[21]=1, bit[10]=1
    // Handles the most common vector operations needed by games.
    // Element operations use memcpy to stay strictly conforming C.
    if ((i & 0x9F200400) == 0x0E200400) {
        const u32 Q    = (i >> 30) & 1;   // 0=64-bit half-register, 1=128-bit full
        const u32 U    = (i >> 29) & 1;
        const u32 size = (i >> 22) & 3;   // 0=B 1=H 2=S 3=D (integer); sz for FP
        const u32 rm   = (i >> 16) & 31;
        const u32 opc5 = (i >> 11) & 31;  // bits[15:11]
        const u32 rn   = (i >> 5)  & 31;
        const u32 rd   = i & 31;
        const int vbytes = Q ? 16 : 8;
        const int esz   = 1 << size;      // bytes per integer element
        const int nelems = vbytes / esz;

        // Bitwise ops (opcode=3): AND/BIC/ORR/ORN (U=0); EOR/BSL/BIT/BIF (U=1).
        // These operate on the full register; element size encodes which variant.
        if (opc5 == 3) {
            // Build body string; if left empty, fall through to unhandled.
            std::string body;
            const std::string vd0 = "c->vreg[" + std::to_string(rd) + "][0]";
            const std::string vd1 = "c->vreg[" + std::to_string(rd) + "][1]";
            const std::string vn0 = "c->vreg[" + std::to_string(rn) + "][0]";
            const std::string vn1 = "c->vreg[" + std::to_string(rn) + "][1]";
            const std::string vm0 = "c->vreg[" + std::to_string(rm) + "][0]";
            const std::string vm1 = "c->vreg[" + std::to_string(rm) + "][1]";
            if (!U) {
                const char* op0 = nullptr; const char* op1 = nullptr;
                if      (size == 0) { op0="&";  op1="&";  }   // AND
                else if (size == 1) { op0="&~"; op1="&~"; }   // BIC
                else if (size == 2) { op0="|";  op1="|";  }   // ORR
                else                { op0="|~"; op1="|~"; }   // ORN
                body = vd0 + "=" + vn0 + op0 + vm0 + "; " + vd1 + "=" + vn1 + op1 + vm1 + "; ";
            } else {
                if (size == 0) {  // EOR
                    body = vd0 + "=" + vn0 + "^" + vm0 + "; " + vd1 + "=" + vn1 + "^" + vm1 + "; ";
                } else if (size == 1) {  // BSL: Vd = (Vd & Vn) | (~Vd & Vm)
                    body = vd0 + "=(" + vd0 + "&" + vn0 + ")|(~" + vd0 + "&" + vm0 + "); ";
                    body += vd1 + "=(" + vd1 + "&" + vn1 + ")|(~" + vd1 + "&" + vm1 + "); ";
                }
                // BIT (size=2) / BIF (size=3): leave on fallback
            }
            if (!body.empty()) {
                std::string s = "{ " + body;
                if (!Q) s += vd1 + "=0; ";
                s += "}";
                put(s);
                return true;
            }
        }

        // Integer ADD (opc5=16, U=0) and SUB (opc5=16, U=1), element-wise.
        if (opc5 == 16) {
            const char* iop = U ? "-" : "+";
            const int b8 = 8 * esz;
            const std::string ty = "uint" + std::to_string(b8) + "_t";
            std::string s = "{ " + ty + " _a[" + std::to_string(nelems) + "],_b[" + std::to_string(nelems) + "],_r[" + std::to_string(nelems) + "]; ";
            s += "memcpy(_a,c->vreg[" + std::to_string(rn) + "]," + std::to_string(vbytes) + "); ";
            s += "memcpy(_b,c->vreg[" + std::to_string(rm) + "]," + std::to_string(vbytes) + "); ";
            s += "for(int _i=0;_i<" + std::to_string(nelems) + ";_i++) _r[_i]=(" + ty + ")(_a[_i]" + iop + "_b[_i]); ";
            s += "memcpy(c->vreg[" + std::to_string(rd) + "],_r," + std::to_string(vbytes) + "); ";
            if (!Q) s += "c->vreg[" + std::to_string(rd) + "][1]=0; ";
            s += "}";
            put(s);
            return true;
        }

        // MUL (opcode 10011, U=0) - element-wise multiply, integer only.
        // U=1 at the same opcode is PMUL (polynomial), which is not the same
        // operation and stays on the fallback.
        if (opc5 == 0x13 && !U && size < 3) {
            const int b8 = 8 * esz;
            const std::string ty = "uint" + std::to_string(b8) + "_t";
            std::string s = "{ " + ty + " _a[" + std::to_string(nelems) + "],_b[" + std::to_string(nelems) + "],_r[" + std::to_string(nelems) + "]; ";
            s += "memcpy(_a,c->vreg[" + std::to_string(rn) + "]," + std::to_string(vbytes) + "); ";
            s += "memcpy(_b,c->vreg[" + std::to_string(rm) + "]," + std::to_string(vbytes) + "); ";
            s += "for(int _i=0;_i<" + std::to_string(nelems) + ";_i++) _r[_i]=(" + ty + ")(_a[_i]*_b[_i]); ";
            s += "memcpy(c->vreg[" + std::to_string(rd) + "],_r," + std::to_string(vbytes) + "); ";
            if (!Q) s += "c->vreg[" + std::to_string(rd) + "][1]=0; ";
            s += "}";
            put(s);
            return true;
        }

        // FP three-register same. Here the two "size" bits mean something
        // different from the integer forms: bit 23 is an opcode-extension bit
        // (it picks FSUB over FADD, FMLS over FMLA) and bit 22 is sz, the
        // element width. Treating bit 23 as part of an element size is what
        // makes FADD look like an unrelated instruction, so the two are split
        // apart explicitly here.
        {
            const u32 a   = (i >> 23) & 1;   // opcode extension, not a size bit
            const bool dbl = ((i >> 22) & 1) != 0;   // sz: 0 = float, 1 = double
            const char* ct = dbl ? "double" : "float";
            const int fsz  = dbl ? 8 : 4;
            const int fne  = vbytes / fsz;
            // Emit an element-wise FP operation, optionally accumulating into rd.
            auto fp_op = [&](const char* expr, bool acc) {
                std::string s = "{ ";
                s += std::string(ct) + " _a[" + std::to_string(fne) + "]";
                s += ",_b[" + std::to_string(fne) + "]";
                if (acc) s += ",_d[" + std::to_string(fne) + "]";
                s += ",_r[" + std::to_string(fne) + "]; ";
                s += "memcpy(_a,c->vreg[" + std::to_string(rn) + "]," + std::to_string(vbytes) + "); ";
                s += "memcpy(_b,c->vreg[" + std::to_string(rm) + "]," + std::to_string(vbytes) + "); ";
                if (acc) s += "memcpy(_d,c->vreg[" + std::to_string(rd) + "]," + std::to_string(vbytes) + "); ";
                s += "for(int _i=0;_i<" + std::to_string(fne) + ";_i++) _r[_i]=(" + std::string(ct) + ")(" + expr + "); ";
                s += "memcpy(c->vreg[" + std::to_string(rd) + "],_r," + std::to_string(vbytes) + "); ";
                if (!Q) s += "c->vreg[" + std::to_string(rd) + "][1]=0; ";
                s += "}";
                put(s);
            };
            // opcode 11001: FMLA (a=0) / FMLS (a=1), both U=0.
            if (opc5 == 0x19 && !U) {
                fp_op(a ? "_d[_i]-_a[_i]*_b[_i]" : "_d[_i]+_a[_i]*_b[_i]", true);
                return true;
            }
            // opcode 11010: FADD (a=0) / FSUB (a=1), both U=0.
            if (opc5 == 0x1A && !U) {
                fp_op(a ? "_a[_i]-_b[_i]" : "_a[_i]+_b[_i]", false);
                return true;
            }
            // opcode 11011 with U=1 and a=0 is FMUL. The U=0 encoding at the
            // same opcode is FMULX, which differs on infinity times zero, so
            // it is deliberately left alone rather than aliased to FMUL.
            if (opc5 == 0x1B && U && !a) {
                fp_op("_a[_i]*_b[_i]", false);
                return true;
            }
            // opcode 11111 with U=1 and a=0 is FDIV.
            if (opc5 == 0x1F && U && !a) {
                fp_op("_a[_i]/_b[_i]", false);
                return true;
            }
        }
    }

    // SIMD/FP load/store pair with V=1 (LDP/STP for S, D, Q registers).
    // Same general format as the integer pair handler, but with SIMD registers.
    // Sizes: opc=00→32-bit(S), opc=01→64-bit(D), opc=10→128-bit(Q).
    if ((i & 0x3E000000) == 0x2C000000) {
        const u32 opc = i >> 30;
        const bool is_load = (i >> 22) & 1;
        const u32 mode = (i >> 23) & 3;    // 1=post, 2=signed-offset, 3=pre
        const u32 rt2 = (i >> 10) & 31, rn = (i >> 5) & 31, rt = i & 31;
        s32 imm7 = (s32)((i >> 15) & 0x7F);
        if (imm7 & 0x40) imm7 |= ~0x7F;
        if ((opc <= 2) && mode >= 1 && mode <= 3) {
            const u32 sz = opc == 0 ? 4 : opc == 1 ? 8 : 16;
            const s64 off = (s64)imm7 * (s64)sz;
            const int bits = sz * 8;
            std::string s = "{ uint64_t _b=c->x[" + std::to_string(rn) + "]; int64_t _o=" + std::to_string((long long)off) + "; ";
            // std::string, not const char*: this gets concatenated with byte
            // offsets below, and as a pointer that silently becomes pointer
            // arithmetic on the literal rather than building an expression.
            const std::string addr = (mode == 1) ? std::string("_b") : std::string("(_b+_o)");
            if (is_load) {
                if (sz <= 8) {
                    s += "{ uint64_t _v=recomp_load" + std::to_string(bits) + "(c," + addr + "); memcpy(&c->vreg[" + std::to_string(rt) + "][0],&_v," + std::to_string(sz) + "); c->vreg[" + std::to_string(rt) + "][1]=0; }";
                    s += "{ uint64_t _v=recomp_load" + std::to_string(bits) + "(c," + addr + "+" + std::to_string(sz) + "); memcpy(&c->vreg[" + std::to_string(rt2) + "][0],&_v," + std::to_string(sz) + "); c->vreg[" + std::to_string(rt2) + "][1]=0; }";
                } else {
                    // 128-bit: two 64-bit loads per register
                    s += "{ c->vreg[" + std::to_string(rt) + "][0]=recomp_load64(c," + addr + "); c->vreg[" + std::to_string(rt) + "][1]=recomp_load64(c," + addr + "+8); }";
                    s += "{ c->vreg[" + std::to_string(rt2) + "][0]=recomp_load64(c," + addr + "+16); c->vreg[" + std::to_string(rt2) + "][1]=recomp_load64(c," + addr + "+24); }";
                }
            } else {
                if (sz <= 8) {
                    s += "{ uint64_t _v=0; memcpy(&_v,&c->vreg[" + std::to_string(rt) + "][0]," + std::to_string(sz) + "); recomp_store" + std::to_string(bits) + "(c," + addr + ",_v); }";
                    s += "{ uint64_t _v=0; memcpy(&_v,&c->vreg[" + std::to_string(rt2) + "][0]," + std::to_string(sz) + "); recomp_store" + std::to_string(bits) + "(c," + addr + "+" + std::to_string(sz) + ",_v); }";
                } else {
                    s += "{ recomp_store64(c," + addr + ",c->vreg[" + std::to_string(rt) + "][0]); recomp_store64(c," + addr + "+8,c->vreg[" + std::to_string(rt) + "][1]); }";
                    s += "{ recomp_store64(c," + addr + "+16,c->vreg[" + std::to_string(rt2) + "][0]); recomp_store64(c," + addr + "+24,c->vreg[" + std::to_string(rt2) + "][1]); }";
                }
            }
            if (mode == 1 || mode == 3) s += "c->x[" + std::to_string(rn) + "]=_b+_o; ";
            s += "}";
            put(s);
            return true;
        }
    }

    // SIMD/FP single-register load and store: the V=1 counterparts of the
    // integer forms handled further up. Access width is a scale value where
    // 0..3 mean 1/2/4/8 bytes and 4 means a whole 16-byte Q register; the
    // 16-byte case is encoded as size==00 with the high bit of opc set, which
    // is why the width cannot simply be read off the size field.
    const auto simd_scale = [](u32 size, u32 opc) {
        return (size == 0 && (opc & 2)) ? 4 : (int)size;
    };
    const auto simd_mem = [&](u32 rt, const std::string& ea, int scale, bool is_load) {
        const int nb = 1 << scale;
        const std::string v0 = "c->vreg[" + std::to_string(rt) + "][0]";
        const std::string v1 = "c->vreg[" + std::to_string(rt) + "][1]";
        if (is_load) {
            if (nb == 16) {
                return v0 + "=recomp_load64(c," + ea + "); " + v1 + "=recomp_load64(c,(" + ea + ")+8); ";
            }
            // Narrower loads zero the rest of the register, as the architecture
            // requires - the destination is written whole, not merged into.
            return "{ uint64_t _v=recomp_load" + std::to_string(nb * 8) + "(c," + ea + "); " +
                   v0 + "=_v; " + v1 + "=0; } ";
        }
        if (nb == 16) {
            return "recomp_store64(c," + ea + "," + v0 + "); recomp_store64(c,(" + ea + ")+8," + v1 + "); ";
        }
        return "{ uint64_t _v=0; memcpy(&_v,&" + v0 + "," + std::to_string(nb) + "); recomp_store" +
               std::to_string(nb * 8) + "(c," + ea + ",_v); } ";
    };

    // LDR/STR (SIMD, register offset): [Xn, Xm{, extend {amount}}].
    if ((i & 0x3F200C00) == 0x3C200800) {
        const u32 size = i >> 30, opc = (i >> 22) & 3;
        const u32 rn = (i >> 5) & 31, rt = i & 31;
        const u32 option = (i >> 13) & 7, S = (i >> 12) & 1;
        const int scale = simd_scale(size, opc);
        // Index register 31 is XZR, not SP.
        const std::string rm_str = Xz((i >> 16) & 31);
        std::string idx;
        switch (option) {
        case 2: idx = "(uint64_t)(uint32_t)" + rm_str; break;                      // UXTW
        case 3: idx = rm_str; break;                                                // LSL/UXTX
        case 6: idx = "(uint64_t)(int64_t)(int32_t)(uint32_t)" + rm_str; break;    // SXTW
        case 7: idx = rm_str; break;                                                // SXTX
        default: break;
        }
        if (!idx.empty()) {
            // The shift amount, when enabled, is the access scale - not the
            // size field, which disagrees with it for the 16-byte form.
            if (S && scale) idx = "((" + idx + ")<<" + std::to_string(scale) + ")";
            const std::string ea = "(c->x[" + std::to_string(rn) + "]+" + idx + ")";
            put("{ " + simd_mem(rt, ea, scale, (opc & 1) != 0) + "}");
            return true;
        }
    }

    // LDR/STR (SIMD, unsigned 12-bit immediate offset) - the commonest form.
    if ((i & 0x3F000000) == 0x3D000000) {
        const u32 size = i >> 30, opc = (i >> 22) & 3;
        const u32 imm12 = (i >> 10) & 0xFFF, rn = (i >> 5) & 31, rt = i & 31;
        const int scale = simd_scale(size, opc);
        const std::string ea = "(c->x[" + std::to_string(rn) + "]+" +
                               std::to_string((unsigned long long)imm12 << scale) + "ULL)";
        put("{ " + simd_mem(rt, ea, scale, (opc & 1) != 0) + "}");
        return true;
    }

    // LDUR/STUR (SIMD) and the pre/post-indexed immediate forms.
    if ((i & 0x3F000000) == 0x3C000000 && ((i >> 24) & 1) == 0) {
        const u32 size = i >> 30, opc = (i >> 22) & 3, mode = (i >> 10) & 3;
        const u32 rn = (i >> 5) & 31, rt = i & 31;
        s32 imm9 = (s32)((i >> 12) & 0x1FF);
        if (imm9 & 0x100) imm9 |= ~0x1FF;
        if (mode == 0 || mode == 1 || mode == 3) {
            const int scale = simd_scale(size, opc);
            std::string s = "{ uint64_t _b=c->x[" + std::to_string(rn) + "]; int64_t _o=" +
                            std::to_string((long long)imm9) + "; ";
            // Post-index accesses the unmodified base; the other two add the
            // offset first. Pre- and post-index both write the base back.
            const std::string ea = (mode == 1) ? "_b" : "(_b+_o)";
            s += simd_mem(rt, ea, scale, (opc & 1) != 0);
            if (mode == 1 || mode == 3) s += "c->x[" + std::to_string(rn) + "]=_b+_o; ";
            s += "}";
            put(s);
            return true;
        }
    }

    // An instruction the decoder doesn't know is reported and stepped over
    // rather than ending the block. recomp_unhandled is a non-fatal stub, and
    // ending the block here would set pc to the following instruction - an
    // address that block discovery never marked as a block start, so the
    // dispatcher could not resolve it and execution would stop dead at the
    // first unimplemented opcode instead of continuing past it.
    // Leaving the block the moment the handler halts is what makes the
    // hand-off to the fallback engine correct: the remaining instructions in
    // this block must not run twice, since the fallback resumes at this same
    // PC and will execute them itself.
    snprintf(buf, sizeof buf,
             "recomp_unhandled(c,0x%08xU,g_module_base+0x%llxULL); if(c->halted) return;", i,
             (unsigned long long)pc);
    put(buf); return true;
}

inline std::string FuncName(const std::string& mod, u64 v) {
    char b[64]; snprintf(b, sizeof b, "blk_%s_%016llx", mod.c_str(), (unsigned long long)v); return b;
}

// Same name, written into a caller-owned buffer. The emit loops call this once
// per block for the body and twice more per block for the dispatch table, so on
// a multi-million-block title the returned-std::string form above is millions of
// heap allocations for a name that is always well under 64 bytes.
inline size_t FuncNameTo(char (&b)[64], const char* mod, u64 v) {
    return (size_t)snprintf(b, sizeof b, "blk_%s_%016llx", mod, (unsigned long long)v);
}

const char* RuntimeH();
const char* RuntimeC();

// Stats returned to the caller for manifest/reporting.
struct RecompileStats {
    size_t blocks = 0;
    size_t instructions = 0;
    size_t translated_terminators = 0;
};

// Emit a buildable C project that statically recompiles `text` (raw AArch64 .text at `base`).
// Layout written into out_dir:
//   CMakeLists.txt, main.c, recomp_export.c, recomp_runtime.{c,h}   <- hand-readable top level
//   src/recompiled_<mod>.c, src/recompiled_<mod>_<n>.c              <- generated translation units
//   data/{text,rodata,data}.bin                                     <- bundled guest segments
//   build/                                                          <- cmake's one canonical build dir
// The translation units are kept in src/ rather than the module root: a large
// title emits hundreds of them, and loose in the root they bury the four files
// anyone actually needs to look at.
// Optional rodata/data parameters bundle those segments so the exported exe is self-contained.
inline RecompileStats EmitProject(const std::string& mod, const u8* text, size_t n_bytes, u64 base,
                                  const std::string& out_dir, bool source_only,
                                  const u8* rodata = nullptr, size_t rodata_size = 0,
                                  const u8* data_seg = nullptr, size_t data_size = 0,
                                  // Guest address to begin executing at. Defaults to `base`, but a
                                  // real NSO starts with a MOD0 header rather than code, so the
                                  // loader passes the module's actual entry here.
                                  u64 entry_pc = 0,
                                  // Human-readable game name, shown by the generated executable so
                                  // it identifies itself rather than printing raw addresses.
                                  const std::string& display_title = {},
                                  // Addresses of this module's exported dynsym symbols, so
                                  // block discovery seeds a root at each even when nothing in
                                  // this module's own .text branches there directly.
                                  const std::vector<u64>* extra_roots = nullptr) {
    RecompileStats stats;
    auto blocks = DiscoverBlocks(text, n_bytes, base, entry_pc, extra_roots);
    const u32* p = reinterpret_cast<const u32*>(text);
    stats.blocks = blocks.size();
    stats.instructions = n_bytes / 4;

    // Block bodies are split across several translation units. A whole game is
    // millions of blocks, which as one .c file runs to hundreds of megabytes -
    // enough that a compiler needs hours and a great deal of memory, or gives
    // up outright. Splitting keeps each unit to a sane size and lets the build
    // compile them in parallel.
    constexpr size_t kBlocksPerUnit = 20000;
    const size_t unit_count =
        blocks.empty() ? 1 : (blocks.size() + kBlocksPerUnit - 1) / kBlocksPerUnit;
    // Written straight to disk rather than buffered. A whole module is over a
    // gigabyte of C, and holding every unit in memory before writing meant the
    // exporter needed that much RAM on top of the game's own data - enough that
    // exporting a large title died partway, after the biggest module and before
    // the rest, leaving an incomplete and inconsistent set of images behind.
    const auto make_dir = [](const std::string& p) {
        std::error_code ec;
        std::filesystem::create_directories(Utf8Path(p), ec);
    };
    make_dir(out_dir);
    // Every generated translation unit lands here, keeping the module root down
    // to the few files a human reads.
    const std::string src_dir = out_dir + "/src";
    make_dir(src_dir);
    std::vector<std::ofstream> units;
    units.reserve(unit_count);
    for (size_t u = 0; u < unit_count; ++u) {
        units.emplace_back(Utf8Path(src_dir + "/recompiled_" + mod + "_" + std::to_string(u) + ".c"),
                           std::ios::binary);
        units.back() << "/* auto-generated by suyu static recompiler - DO NOT EDIT */\n"
                        "#include \"recomp_runtime.h\"\n"
                        "#include <stdint.h>\n"
                        "#include <string.h>\n"   // memcpy, used by the scalar FP paths
                        "#include <math.h>\n"
                        "struct _recomp_ent{uint64_t va; BlockFn fn;};\n\n";
    }

    // Each unit accumulates into a string and is written out in large blocks.
    // Streaming every fragment through operator<< meant a sentry construction
    // and a virtual xsputn per token (several per instruction) straight into a
    // default 4 KiB filebuf; batching turns the whole emit into a handful of
    // multi-megabyte writes per unit at no cost in output bytes.
    // Blocks are visited in unit order, so a single buffer serves every unit -
    // one buffer per unit would cost hundreds of megabytes of slack on a title
    // that emits hundreds of units.
    constexpr size_t kUnitFlushBytes = 8u << 20;
    std::string rcu;
    rcu.reserve(kUnitFlushBytes + (1u << 20));
    const auto flush_unit = [&](size_t u, bool force) {
        if (rcu.size() >= kUnitFlushBytes || (force && !rcu.empty())) {
            units[u].write(rcu.data(), (std::streamsize)rcu.size());
            rcu.clear();
        }
    };

    size_t block_index = 0;
    // Hoisted out of the per-instruction loop: a fresh std::string per guest
    // instruction is one allocation per instruction across the whole title.
    std::string body;
    body.reserve(4096);
    char namebuf[64];

    // Each unit carries the dispatch-table entries for its own blocks. Putting
    // the whole table in one file instead meant a single translation unit that
    // grew with the module - hundreds of megabytes of forward declarations and
    // one array initialiser for a large title, compiled by one cl.exe with no
    // parallelism and enormous peak memory, which is both the longest pole in
    // the build and a plausible way for it to die outright. Here the entries
    // sit next to the definitions they point at, so they cost no extra
    // declarations and compile across every core with the bodies.
    std::vector<u64> seg_hi(unit_count, 0);
    const auto emit_seg = [&](size_t u) {
        const size_t lo = u * kBlocksPerUnit;
        size_t hi = lo + kBlocksPerUnit;
        if (hi > blocks.size()) {
            hi = blocks.size();
        }
        char line[160];
        snprintf(line, sizeof line, "\nconst struct _recomp_ent _seg_%s_%zu[] = {\n", mod.c_str(),
                 u);
        rcu += line;
        for (size_t i = lo; i < hi; ++i) {
            FuncNameTo(namebuf, mod.c_str(), blocks[i].vaddr);
            snprintf(line, sizeof line, "  {0x%llxULL, %s},\n",
                     (unsigned long long)blocks[i].vaddr, namebuf);
            rcu += line;
        }
        // C has no empty initialiser list; a zero-length segment still needs a
        // well-formed array, and its declared count of 0 keeps it unsearchable.
        if (lo >= hi) {
            rcu += "  {0ULL, 0}\n";
        } else {
            seg_hi[u] = blocks[hi - 1].vaddr;
        }
        snprintf(line, sizeof line, "};\nconst unsigned _segn_%s_%zu = %zuU;\n", mod.c_str(), u,
                 hi - lo);
        rcu += line;
    };

    size_t cur_unit = 0;
    for (const auto& b : blocks) {
        const size_t unit = block_index / kBlocksPerUnit;
        if (unit != cur_unit) {
            emit_seg(cur_unit);
            flush_unit(cur_unit, true);
            cur_unit = unit;
        }
        ++block_index;
        FuncNameTo(namebuf, mod.c_str(), b.vaddr);
        rcu += "void ";
        rcu += namebuf;
        rcu += "(GuestContext* c){\n";
        const u32 first = (u32)((b.vaddr - base) / 4);
        bool open = true;
        for (u32 k = 0; k < b.count; ++k) {
            body.clear();
            open = Translate(p[first + k], b.vaddr + (u64)k * 4, body);
            rcu += body;
            if (!open) { ++stats.translated_terminators; break; }
        }
        // A block that ends by running off its own end still has to hand the
        // dispatcher an absolute guest address, exactly as every branch
        // terminator above does. Emitting the bare module-relative offset here
        // was silently catastrophic: the host dispatcher picks the owning image
        // by "greatest base not exceeding the PC", so a small unbased value has
        // no owner at all and falls through to the "try every image at the raw
        // PC" path - which happily returns *some other module's* block at the
        // same offset and runs it against this module's register state.
        if (open) {
            char tail[80];
            snprintf(tail, sizeof tail, "    c->pc=g_module_base+0x%llxULL; return;\n",
                     (unsigned long long)(b.vaddr + b.size));
            rcu += tail;
        }
        rcu += "}\n\n";
        flush_unit(unit, false);
    }
    emit_seg(cur_unit);
    flush_unit(cur_unit, true);

    // The dispatch table is now just a directory of the per-unit segments
    // emitted above: a few lines per unit instead of two entries per block, so
    // this file stays a few kilobytes no matter how large the module is.
    std::string rc;
    rc.reserve(unit_count * 200 + 4096);
    rc += "/* auto-generated by suyu static recompiler - DO NOT EDIT */\n"
          "#include \"recomp_runtime.h\"\n#include <stdint.h>\n\n"
          "struct _recomp_ent{uint64_t va; BlockFn fn;};\n\n";
    {
        char line[192];
        for (size_t u = 0; u < unit_count; ++u) {
            snprintf(line, sizeof line,
                     "extern const struct _recomp_ent _seg_%s_%zu[];\nextern const unsigned "
                     "_segn_%s_%zu;\n",
                     mod.c_str(), u, mod.c_str(), u);
            rc += line;
        }
        rc += "\nstatic const struct _recomp_ent* const _segs[] = {\n";
        for (size_t u = 0; u < unit_count; ++u) {
            snprintf(line, sizeof line, "  _seg_%s_%zu,\n", mod.c_str(), u);
            rc += line;
        }
        rc += "};\nstatic const unsigned* const _segn[] = {\n";
        for (size_t u = 0; u < unit_count; ++u) {
            snprintf(line, sizeof line, "  &_segn_%s_%zu,\n", mod.c_str(), u);
            rc += line;
        }
        // Highest guest address in each segment. Segments follow the block list,
        // which is sorted ascending, so they are disjoint and ordered - this
        // lets the lookup binary-search for the owning segment rather than
        // walking every one of them on each dispatch.
        rc += "};\nstatic const uint64_t _seg_hi[] = {\n";
        for (size_t u = 0; u < unit_count; ++u) {
            snprintf(line, sizeof line, "  0x%llxULL,\n", (unsigned long long)seg_hi[u]);
            rc += line;
        }
        rc += "};\n";
    }
    rc += "#define _NSEG (sizeof(_segs)/sizeof(_segs[0]))\n"
          "BlockFn recomp_lookup(uint64_t pc){\n"
          "  unsigned slo=0, shi=(unsigned)_NSEG;\n"
          "  while(slo<shi){ unsigned m=slo+(shi-slo)/2; if(_seg_hi[m]<pc) slo=m+1; else shi=m; }\n"
          "  if(slo>=_NSEG) return 0;\n"
          "  {\n"
          "    const struct _recomp_ent* t=_segs[slo];\n"
          "    unsigned n=*_segn[slo], lo=0, hi=n;\n"
          "    while(lo<hi){ unsigned m=lo+(hi-lo)/2; if(t[m].va<pc) lo=m+1; else hi=m; }\n"
          "    return (lo<n && t[lo].va==pc)?t[lo].fn:0;\n"
          "  }\n}\n";

    const std::string title_str = display_title.empty() ? mod : display_title;
    std::ostringstream mc;
    mc << "#include \"recomp_runtime.h\"\n#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n";
    mc << "#ifdef HAVE_SDL2\n#include <SDL2/SDL.h>\n#endif\n\n";
    mc << "#define GUEST_MEM_SIZE (256ULL * 1024 * 1024) /* 256 MB */\n\n";
    mc << "static const char* kGameTitle = \"" << title_str << "\";\n\n";
    mc << "#ifdef HAVE_SDL2\n";
    mc << "static SDL_Window*   g_sdl_window   = NULL;\n";
    mc << "static SDL_Renderer* g_sdl_renderer = NULL;\n";
    mc << "static SDL_Texture*  g_sdl_texture  = NULL;\n";
    mc << "static int g_fb_w = 1280, g_fb_h = 720;\n\n";
    mc << "/* Called from the SVC handler when the guest flushes a framebuffer.\n";
    mc << "   fb: CPU-accessible RGBA8 pixels, w x h. */\n";
    mc << "void recomp_sdl_blit(const void* fb, int w, int h) {\n";
    mc << "  if(!g_sdl_renderer) return;\n";
    mc << "  if(w!=g_fb_w || h!=g_fb_h || !g_sdl_texture) {\n";
    mc << "    if(g_sdl_texture) SDL_DestroyTexture(g_sdl_texture);\n";
    mc << "    g_sdl_texture = SDL_CreateTexture(g_sdl_renderer,\n";
    mc << "      SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, w, h);\n";
    mc << "    g_fb_w=w; g_fb_h=h;\n";
    mc << "  }\n";
    mc << "  SDL_UpdateTexture(g_sdl_texture, NULL, fb, w*4);\n";
    mc << "  SDL_RenderClear(g_sdl_renderer);\n";
    mc << "  SDL_RenderCopy(g_sdl_renderer, g_sdl_texture, NULL, NULL);\n";
    mc << "  SDL_RenderPresent(g_sdl_renderer);\n}\n\n";
    mc << "static int sdl_pump_events(void) {\n";
    mc << "  SDL_Event e;\n";
    mc << "  while(SDL_PollEvent(&e)) {\n";
    mc << "    if(e.type==SDL_QUIT) return 0;\n";
    mc << "    if(e.type==SDL_KEYDOWN && e.key.keysym.sym==SDLK_ESCAPE) return 0;\n";
    mc << "  }\n  return 1;\n}\n#endif /* HAVE_SDL2 */\n\n";
    mc << "int main(int argc, char** argv){\n";
    mc << "  printf(\"=== %s ===\\n\", kGameTitle);\n\n";
    mc << "#ifdef HAVE_SDL2\n";
    mc << "  if(SDL_Init(SDL_INIT_VIDEO) == 0) {\n";
    mc << "    g_sdl_window = SDL_CreateWindow(kGameTitle,\n";
    mc << "      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_RESIZABLE);\n";
    mc << "    if(g_sdl_window)\n";
    mc << "      g_sdl_renderer = SDL_CreateRenderer(g_sdl_window, -1, SDL_RENDERER_ACCELERATED);\n";
    mc << "  } else {\n";
    mc << "    fprintf(stderr, \"[recomp] SDL2 init failed: %s (running headless)\\n\", SDL_GetError());\n";
    mc << "  }\n#endif\n\n";
    mc << "  uint8_t* mem = (uint8_t*)calloc(1, (size_t)GUEST_MEM_SIZE);\n";
    mc << "  if(!mem){ fprintf(stderr,\"Failed to allocate guest memory\\n\"); return 1; }\n";
    mc << "  GuestContext c; memset(&c,0,sizeof c);\n";
    mc << "  c.mem=mem; c.mem_size=GUEST_MEM_SIZE;\n";
    mc << "  c.mem_base_vaddr=0x" << std::hex << base << std::dec << "ULL;\n";
    mc << "  c.heap_base=c.mem_base_vaddr+GUEST_MEM_SIZE/2;\n";
    mc << "  c.heap_cur=c.heap_base; c.heap_end=c.mem_base_vaddr+GUEST_MEM_SIZE;\n";
    mc << "  c.x[31]=c.mem_base_vaddr + GUEST_MEM_SIZE - 16; /* SP */\n";
    mc << "  c.pc=0x" << std::hex << (entry_pc ? entry_pc : base) << std::dec << "ULL;\n\n";
    mc << "  recomp_save_init(&c, argv[0]);\n\n";
    mc << "  { char data_dir[512];\n";
    mc << "    snprintf(data_dir,sizeof data_dir,\"%s\",argv[0]);\n";
    mc << "    char* sl=strrchr(data_dir,'\\\\'); if(!sl) sl=strrchr(data_dir,'/'); if(sl) *(sl+1)=0; else data_dir[0]=0;\n";
    mc << "    strncat(data_dir,\"data\",sizeof(data_dir)-strlen(data_dir)-1);\n";
    mc << "    recomp_load_segments(&c,data_dir);\n  }\n\n";
    mc << "  { uint64_t sz=0;\n";
    mc << "    if(recomp_save_exists(&c,\"autosave.bin\")){\n";
    mc << "      recomp_save_read(&c,\"autosave.bin\",c.mem,(uint64_t)GUEST_MEM_SIZE,&sz);\n";
    mc << "      printf(\"[recomp] Restored autosave (%llu bytes)\\n\",(unsigned long long)sz);\n";
    mc << "    }\n  }\n\n";
    mc << "  printf(\"[recomp] Starting at pc=0x%llx\\n\",(unsigned long long)c.pc);\n";
    mc << "  /* Main loop: pump SDL events while the guest runs */\n";
    mc << "#ifdef HAVE_SDL2\n";
    mc << "  while(!c.halted) {\n";
    mc << "    if(!sdl_pump_events()) break;\n";
    mc << "    recomp_run(&c); /* runs until SVC or halt */\n";
    mc << "  }\n";
    mc << "#else\n";
    mc << "  recomp_run(&c);\n";
    mc << "#endif\n\n";
    mc << "  recomp_save_write(&c,\"autosave.bin\",c.mem,(uint64_t)GUEST_MEM_SIZE);\n";
    mc << "  printf(\"[recomp] halted at pc=0x%llx\\n\",(unsigned long long)c.pc);\n";
    mc << "#ifdef HAVE_SDL2\n";
    mc << "  if(g_sdl_texture)  SDL_DestroyTexture(g_sdl_texture);\n";
    mc << "  if(g_sdl_renderer) SDL_DestroyRenderer(g_sdl_renderer);\n";
    mc << "  if(g_sdl_window)   SDL_DestroyWindow(g_sdl_window);\n";
    mc << "  SDL_Quit();\n";
    mc << "#endif\n";
    mc << "  free(mem);\n  return 0;\n}\n";

    std::ostringstream cm;
    // The generated units live in src/ and include "recomp_runtime.h" from the
    // module root, so the root has to be on the include path.
    cm << "cmake_minimum_required(VERSION 3.13)\n"
       << "# When this directory is pulled into a bigger build (suyu-cmd linking the\n"
       << "# modules statically) it must not start a project of its own - it just\n"
       << "# contributes targets. Standalone it is still a complete project.\n"
       << "if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)\n"
       << "  project(suyu_recompiled C)\n"
       << "else()\n"
       << "  enable_language(C)\n"
       << "endif()\n"
       << "set(CMAKE_C_STANDARD 11)\n"
       << "include_directories(${CMAKE_CURRENT_SOURCE_DIR})\n"
       << "# A host project (suyu) may apply C++ flags to every language; this\n"
       << "# directory is plain C, and MSVC rejects /std:c11 together with\n"
       << "# /std:c++20 outright.\n"
       << "get_directory_property(_recomp_opts COMPILE_OPTIONS)\n"
       << "if(_recomp_opts)\n"
       << "  list(FILTER _recomp_opts EXCLUDE REGEX \"std:c\\\\+\\\\+|std=c\\\\+\\\\+|EHsc|permissive\")\n"
       << "  set_directory_properties(PROPERTIES COMPILE_OPTIONS \"${_recomp_opts}\")\n"
       << "endif()\n\n"
       << "# RECOMP_STATIC_ONLY: build just the static library this module\n"
       << "# contributes to a host executable, skipping the portable standalone exe\n"
       << "# and the loadable shared image.\n"
       << "if(NOT RECOMP_STATIC_ONLY)\n"
       << "# Optional: SDL2 window for display output.\n"
       << "# Install SDL2 (e.g. vcpkg install sdl2) to enable the game window.\n"
       << "find_package(SDL2 QUIET)\n"
       << "endif()\n\n"
       << "# Generated translation units, all under src/.\nset(RECOMP_SOURCES\n"
       << "    src/recompiled_" << mod << ".c";
    for (size_t u = 0; u < unit_count; ++u) {
        cm << "\n    src/recompiled_" << mod << "_" << u << ".c";
    }
    cm << ")\n\n"
       << "# Generated block bodies are huge flat switch/if chains translated\n"
       << "# straight from machine code - there's no loop nesting or hot path for\n"
       << "# -O2 to meaningfully improve, just a lot of blocks for it to chew\n"
       << "# through. Compiling them at -O1 cuts single-TU compile time sharply\n"
       << "# on large modules with no measurable runtime cost.\n"
       << "if(MSVC)\n"
       // /MP is what actually decides wall-clock time here. The Visual Studio
       // generator compiles the files of a single project strictly in
       // sequence, so a module with 100+ generated translation units serialises
       // onto one core no matter what --parallel is passed to `cmake --build`.
       // /MP fans them out across every core. Ninja parallelises on its own and
       // warns about /MP, so gate it on the generator, not just on MSVC.
       // The warning suppressions are the noisy ones the translation
       // unavoidably produces (constant conditionals, provably-taken
       // divide/shift paths); silencing them keeps cl.exe from spending real
       // time formatting hundreds of thousands of diagnostics.
       // /we4189 (from the top-level target's inherited warning-as-error
       // set) turns "unused local" into a hard build failure; the codegen
       // legitimately computes and drops _r on some flag-only paths, so
       // downgrade it back to a warning for generated sources specifically.
       << "  set(_recomp_msvc_opts \"/O1\" \"/wd4127\" \"/wd4723\" \"/wd4102\" \"/wd4101\" "
          "\"/wd4189\")\n"
       << "  if(NOT CMAKE_GENERATOR MATCHES \"Ninja\")\n"
       << "    list(APPEND _recomp_msvc_opts \"/MP\")\n"
       << "  endif()\n"
       << "  set_source_files_properties(${RECOMP_SOURCES} PROPERTIES COMPILE_OPTIONS "
          "\"${_recomp_msvc_opts}\")\n"
       << "else()\n"
       << "  set_source_files_properties(${RECOMP_SOURCES} PROPERTIES COMPILE_OPTIONS \"-O1\")\n"
       << "endif()\n\n"
       << "if(NOT RECOMP_STATIC_ONLY)\n"
       << "add_executable(recompiled main.c recomp_runtime.c ${RECOMP_SOURCES})\n"
       << "if(SDL2_FOUND)\n"
       << "  target_compile_definitions(recompiled PRIVATE HAVE_SDL2)\n"
       << "  target_include_directories(recompiled PRIVATE ${SDL2_INCLUDE_DIRS})\n"
       << "  target_link_libraries(recompiled ${SDL2_LIBRARIES})\n"
       << "  message(STATUS \"SDL2 found — recompiled will open a game window\")\n"
       << "else()\n"
       << "  message(STATUS \"SDL2 not found — running headless (no window)\")\n"
       << "endif()\n"
       << "# Portable C11: Windows->.exe, Linux/FreeBSD/OpenBSD->ELF, macOS->Mach-O\n"
       << "endif()\n\n";

    // A second target builds the same code as a shared library exporting the
    // block lookup. That is what suyu loads to run this image on
    // Core::ArmRecomp: the emulator supplies memory and services through the
    // host bridge, so main.c - which owns a flat buffer and a stub SVC handler
    // - is deliberately left out of this target.
    //
    // Target/output name is per-module so every NSO's DLL builds side by side
    // without colliding: suyu-cmd's loader (src/suyu_cmd/suyu.cpp) looks for
    // recompiled_rtld.dll / recompiled_image.dll (main) / recompiled_sdk.dll /
    // recompiled_subsdkN.dll next to the exe, in NSO load order.
    const std::string dll_target = (mod == "main") ? "recompiled_image" : ("recompiled_" + mod);
    cm << "if(NOT RECOMP_STATIC_ONLY)\n"
       << "add_library(" << dll_target << " SHARED recomp_export.c recomp_runtime.c "
          "${RECOMP_SOURCES})\n"
       << "set_target_properties(" << dll_target << " PROPERTIES C_VISIBILITY_PRESET hidden "
          "OUTPUT_NAME \"" << dll_target << "\")\n"
       << "target_compile_definitions(" << dll_target
       << " PRIVATE SUYU_HOSTED_RECOMP=1 RECOMP_SHARED_MODULE=1)\n"
       << "endif()\n\n";

    // Static-library variant. Several of these get linked into ONE host
    // executable (the per-game suyu-cmd build), so every symbol a module owns
    // has to be unique. The generated C is written once and renamed at compile
    // time through -D, which keeps the sources identical between the shared and
    // the static shape.
    //
    // recomp_runtime.c is deliberately NOT part of this target: its contents
    // (recomp_svc, the load/store helpers, recomp_cond, ...) are generic and
    // must exist exactly once in the final link. It is built separately, once,
    // as recomp_runtime_shared.
    const std::string static_target = "recomp_static_" + mod;
    cm << "if(NOT TARGET recomp_runtime_shared)\n"
       << "  add_library(recomp_runtime_shared STATIC recomp_runtime.c)\n"
       << "  target_compile_definitions(recomp_runtime_shared PRIVATE SUYU_HOSTED_RECOMP=1 "
          "RECOMP_STATIC_HOST=1)\n"
       << "  target_include_directories(recomp_runtime_shared PUBLIC "
          "${CMAKE_CURRENT_SOURCE_DIR})\n"
       << "endif()\n"
       << "add_library(" << static_target << " STATIC recomp_export.c ${RECOMP_SOURCES})\n"
       << "target_include_directories(" << static_target
       << " PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})\n"
       << "target_link_libraries(" << static_target << " PUBLIC recomp_runtime_shared)\n"
       << "target_compile_definitions(" << static_target
       << " PRIVATE SUYU_HOSTED_RECOMP=1 RECOMP_STATIC_MODULE=1"
       << " g_module_base=g_module_base_" << mod
       << " recomp_lookup=recomp_lookup_" << mod
       << " recomp_image_lookup=recomp_image_lookup_" << mod
       << " recomp_image_set_base=recomp_image_set_base_" << mod
       << " recomp_image_entry=recomp_image_entry_" << mod << ")\n";

    std::ostringstream ex;
    ex << "/* auto-generated by suyu static recompiler - DO NOT EDIT */\n"
          "#include \"recomp_runtime.h\"\n\n"
          "/* The one symbol suyu resolves out of a recompiled image. Named\n"
          "   distinctly from the internal recomp_lookup so the export is\n"
          "   unambiguous, and so the internal one can stay hidden. */\n"
          "#ifdef RECOMP_STATIC_MODULE\n"
          "/* Linked straight into the host executable: static-library symbols are\n"
          "   visible to the linker on their own, and the names are already made\n"
          "   unique per module by the -D renames the build applies. */\n"
          "#define RECOMP_API\n"
          "/* The shared runtime is compiled once for the whole link and therefore\n"
          "   cannot own this - each module needs its own load base. */\n"
          "uint64_t g_module_base = 0;\n"
          "#elif defined(_WIN32)\n"
          "#define RECOMP_API __declspec(dllexport)\n"
          "#else\n"
          "#define RECOMP_API __attribute__((visibility(\"default\")))\n"
          "#endif\n\n"
          "RECOMP_API BlockFn recomp_image_lookup(uint64_t pc){ return recomp_lookup(pc - g_module_base); }\n\n"
          "/* Tells this image where its module actually got loaded, so the\n"
          "   addresses it computes are real rather than module-relative. */\n"
          "RECOMP_API void recomp_image_set_base(uint64_t base){ g_module_base = base; }\n\n"
          "/* Reports the entry PC so the loader does not have to be told it\n"
          "   separately or parse the image again. */\n"
       << "RECOMP_API uint64_t recomp_image_entry(void){ return 0x"
       << std::hex << (entry_pc ? entry_pc : base) << std::dec << "ULL; }\n";

    auto write = [&](const std::string& name, const std::string& data) {
        std::ofstream o(Utf8Path(out_dir + "/" + name), std::ios::binary);
        o.write(data.data(), (std::streamsize)data.size());
    };
    write("recomp_runtime.h", RuntimeH());
    write("recomp_runtime.c", RuntimeC());
    // The dispatch table is generated code like the block bodies, so it belongs
    // with them rather than at the top level.
    write("src/recompiled_" + mod + ".c", rc);
    // The unit files were streamed out as blocks were translated; just make
    // sure everything has reached disk.
    for (auto& u : units) {
        u.flush();
        u.close();
    }
    write("main.c", mc.str());
    write("recomp_export.c", ex.str());
    write("CMakeLists.txt", cm.str());

    // Bundle text/rodata/data as binary blobs so the exe can load them at startup
    {
        std::string data_subdir = out_dir + "/data";
        make_dir(data_subdir);
        // Always write text.bin
        {
            std::ofstream o(Utf8Path(data_subdir + "/text.bin"), std::ios::binary);
            o.write(reinterpret_cast<const char*>(text), (std::streamsize)n_bytes);
        }
        if (rodata && rodata_size > 0) {
            std::ofstream o(Utf8Path(data_subdir + "/rodata.bin"), std::ios::binary);
            o.write(reinterpret_cast<const char*>(rodata), (std::streamsize)rodata_size);
        }
        if (data_seg && data_size > 0) {
            std::ofstream o(Utf8Path(data_subdir + "/data.bin"), std::ios::binary);
            o.write(reinterpret_cast<const char*>(data_seg), (std::streamsize)data_size);
        }
    }

    return stats;
}

inline const char* RuntimeH() {
    return R"RT(#ifndef SUYU_RECOMP_RUNTIME_H
#define SUYU_RECOMP_RUNTIME_H
#include <stdint.h>
#include <stddef.h>   /* offsetof, for the layout assertions below */

/* Supplied by the host when the recompiled image is driven by an emulator
   rather than run standalone. `size` is 1, 2, 4 or 8 bytes. */
typedef struct RecompHostMem {
    void* user;
    uint64_t (*load)(void* user, uint64_t va, uint32_t size);
    void (*store)(void* user, uint64_t va, uint32_t size, uint64_t value);
} RecompHostMem;

typedef struct GuestContext {
    uint64_t x[32]; uint64_t pc; uint8_t n,z,c,v;
    uint8_t* mem; uint64_t mem_size; uint64_t mem_base_vaddr; int halted;
    /* SVC signalling. The emitted code sets this to the instruction's imm
       before calling recomp_svc(). The standalone runtime services the call
       and clears it; when the recompiled code is instead driven by suyu's
       Core::ArmRecomp backend, recomp_svc leaves it set so the emulator can
       see the pending call and dispatch it through the real HLE kernel.
       ~0ULL means "no SVC pending". */
    uint64_t pending_svc;
    /* SIMD/FP register file, 128 bits each held as two 64-bit halves.
       Deliberately placed after pending_svc: Core::ArmRecomp mirrors only the
       prefix of this struct up to that field, so appending here leaves that
       view valid. */
    uint64_t vreg[32][2];
    /* Thread pointer (TPIDR_EL0). Read/written by MRS/MSR; user code uses it
       for thread-local storage. */
    uint64_t tpidr_el0;
    /* Host memory bridge. NULL for the standalone runtime, which owns the flat
       buffer above. When suyu drives this code through Core::ArmRecomp the
       guest's address space belongs to the emulator, not to us, so every
       access has to go back out to Core::Memory instead of into `mem` -
       otherwise the recompiled code and the HLE kernel would be looking at two
       different memories. */
    const struct RecompHostMem* host_mem;
    /* Read-only thread pointer (TPIDRRO_EL0). Distinct from tpidr_el0 above:
       on Horizon the kernel publishes the calling thread's thread-local
       region here, and that region's first 0x100 bytes are the IPC message
       buffer the kernel reads a SendSyncRequest out of. tpidr_el0 in contrast
       belongs to the guest, which points it at its own thread structure.
       Aliasing the two makes the guest write its IPC header at its thread
       struct instead of the TLS region - the kernel then parses an all-zero
       buffer (magic 0 instead of 'SFCI') - and makes the kernel's per-switch
       publish of the TLS address clobber the guest's thread pointer, which
       turns every C++ thread-local access into a near-null read.
       Appended after host_mem so every pinned offset above stays put. */
    uint64_t tpidrro_el0;
    /* Save-data filesystem state */
    char save_dir[512];
    /* Heap break for SVC memory allocation */
    uint64_t heap_base; uint64_t heap_end; uint64_t heap_cur;
    /* IPC command buffer (simplified HLE) */
    uint32_t ipc_cmd[64];
    /* Open file handles for save data (simplified) */
    void* save_handles[16]; int save_handle_count;
} GuestContext;

/* Core::ArmRecomp mirrors the prefix of this struct with its own declaration
   so the emulator can read and write guest state without including this
   header. Nothing enforces that across the two builds, so the offsets are
   pinned here: if a field is inserted rather than appended, this fails loudly
   at generation time instead of silently handing the emulator the wrong
   register file. Keep in sync with GuestContextView in
   core/arm/recomp/arm_recomp.cpp. */
/* A negative array size rather than _Static_assert: the generated project is
   plain C compiled by whatever the user has, and MSVC's /TC mode rejects the
   C11 form outright. This construct is valid in every C dialect. */
typedef char recomp_layout_pc[offsetof(GuestContext, pc) == 256 ? 1 : -1];
typedef char recomp_layout_svc[offsetof(GuestContext, pending_svc) == 304 ? 1 : -1];
typedef char recomp_layout_vreg[offsetof(GuestContext, vreg) == 312 ? 1 : -1];
typedef char recomp_layout_tpidr[offsetof(GuestContext, tpidr_el0) == 824 ? 1 : -1];
typedef char recomp_layout_host[offsetof(GuestContext, host_mem) == 832 ? 1 : -1];
typedef char recomp_layout_tpidrro[offsetof(GuestContext, tpidrro_el0) == 840 ? 1 : -1];

/* Where this module is actually loaded in the guest's address space.
   Every address the static pass bakes in - ADR/ADRP results, branch targets,
   return addresses - is relative to the module, because that is all it can
   know: the loader picks the real base at run time and it differs per run.
   Leaving it zero gives the module-relative behaviour the standalone runtime
   wants; the emulator sets it once at load so computed data pointers land in
   real memory rather than near null. One global per image, since each module
   is its own shared library. */
extern uint64_t g_module_base;

typedef void (*BlockFn)(GuestContext*);
BlockFn recomp_lookup(uint64_t pc); void recomp_run(GuestContext* c);
void recomp_set_flags(GuestContext*,int,uint64_t,uint64_t,uint64_t,int);
/* High 64 bits of a 64x64 multiply, for SMULH/UMULH. */
uint64_t recomp_smulh(uint64_t,uint64_t);
uint64_t recomp_umulh(uint64_t,uint64_t);
int  recomp_cond(GuestContext*,unsigned);
uint64_t recomp_load8(GuestContext*,uint64_t); uint64_t recomp_load16(GuestContext*,uint64_t);
uint64_t recomp_load32(GuestContext*,uint64_t); uint64_t recomp_load64(GuestContext*,uint64_t);
void recomp_store8(GuestContext*,uint64_t,uint64_t); void recomp_store16(GuestContext*,uint64_t,uint64_t);
void recomp_store32(GuestContext*,uint64_t,uint64_t); void recomp_store64(GuestContext*,uint64_t,uint64_t);
void recomp_svc(GuestContext*,unsigned); void recomp_unhandled(GuestContext*,uint32_t,uint64_t);
void recomp_barrier(void);
/* halted values. 1 is an ordinary stop; 2 asks the host to re-execute the
   current PC on its interpreter fallback because the decoder had no
   translation for the instruction there. */
#define RECOMP_HALT_UNHANDLED 2
/* Save-data API callable from recompiled code and runtime */
int  recomp_save_init(GuestContext* c, const char* exe_path);
int  recomp_save_write(GuestContext* c, const char* name, const void* data, uint64_t size);
int  recomp_save_read(GuestContext* c, const char* name, void* buf, uint64_t buf_size, uint64_t* out_size);
int  recomp_save_delete(GuestContext* c, const char* name);
int  recomp_save_exists(GuestContext* c, const char* name);
/* Load bundled data segments into guest memory */
int  recomp_load_segments(GuestContext* c, const char* data_dir);
#endif
)RT";
}

inline const char* RuntimeC() {
    return R"RT(#include "recomp_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#define MKDIR(p) _mkdir(p)
#define PATH_SEP '\\'
#else
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#define MKDIR(p) mkdir(p,0755)
#define PATH_SEP '/'
#endif

static uint8_t* memptr(GuestContext* c, uint64_t va, uint64_t sz){
  uint64_t off=va-c->mem_base_vaddr;
  if(off+sz>c->mem_size) return 0;
  return c->mem+off;
}

/* Every guest access funnels through these two, so the host bridge only has to
   be checked in one place per direction. */
static uint64_t memload(GuestContext* c, uint64_t a, uint32_t sz){
  if(c->host_mem) return c->host_mem->load(c->host_mem->user,a,sz);
  { uint8_t* p=memptr(c,a,sz); uint64_t v=0; if(p)memcpy(&v,p,sz); return v; }
}
static void memstore(GuestContext* c, uint64_t a, uint32_t sz, uint64_t v){
  if(c->host_mem){ c->host_mem->store(c->host_mem->user,a,sz,v); return; }
  { uint8_t* p=memptr(c,a,sz); if(p)memcpy(p,&v,sz); }
}

uint64_t recomp_load8 (GuestContext* c,uint64_t a){return memload(c,a,1);}
uint64_t recomp_load16(GuestContext* c,uint64_t a){return memload(c,a,2);}
uint64_t recomp_load32(GuestContext* c,uint64_t a){return memload(c,a,4);}
uint64_t recomp_load64(GuestContext* c,uint64_t a){return memload(c,a,8);}
void recomp_store8 (GuestContext* c,uint64_t a,uint64_t v){memstore(c,a,1,v);}
void recomp_store16(GuestContext* c,uint64_t a,uint64_t v){memstore(c,a,2,v);}
void recomp_store32(GuestContext* c,uint64_t a,uint64_t v){memstore(c,a,4,v);}
void recomp_store64(GuestContext* c,uint64_t a,uint64_t v){memstore(c,a,8,v);}

#ifndef RECOMP_STATIC_HOST
/* Owned by the runtime in the single-module shapes (standalone exe, loadable
   shared image). When several modules are linked statically into one host this
   file is compiled once for all of them, so each module defines its own
   (renamed) copy in recomp_export.c instead. */
uint64_t g_module_base = 0;
#endif

uint64_t recomp_umulh(uint64_t a,uint64_t b){
  /* Portable 64x64->high64: split into 32-bit halves. Avoids depending on
     __int128 or MSVC intrinsics so the generated project stays plain C11. */
  uint64_t al=a&0xFFFFFFFFULL, ah=a>>32, bl=b&0xFFFFFFFFULL, bh=b>>32;
  uint64_t ll=al*bl, lh=al*bh, hl=ah*bl, hh=ah*bh;
  uint64_t mid=(ll>>32)+(lh&0xFFFFFFFFULL)+(hl&0xFFFFFFFFULL);
  return hh+(lh>>32)+(hl>>32)+(mid>>32);
}
uint64_t recomp_smulh(uint64_t a,uint64_t b){
  uint64_t hi=recomp_umulh(a,b);
  /* Convert the unsigned high half to the signed one. */
  if((int64_t)a<0) hi-=b;
  if((int64_t)b<0) hi-=a;
  return hi;
}
void recomp_set_flags(GuestContext* c,int is_sub,uint64_t a,uint64_t b,uint64_t r,int is64){
  uint64_t m=is64?~0ULL:0xFFFFFFFFULL; r&=m;a&=m;b&=m;
  uint64_t s=is64?0x8000000000000000ULL:0x80000000ULL;
  c->z=(r==0); c->n=(r&s)?1:0;
  if(is_sub){ c->c=(a>=b); c->v=(((a^b)&(a^r))&s)?1:0; }
  else { c->c=(r<a); c->v=((~(a^b)&(a^r))&s)?1:0; }
}

int recomp_cond(GuestContext* c,unsigned cond){
  int n=c->n,z=c->z,cc=c->c,v=c->v,res;
  switch(cond>>1){case 0:res=z;break;case 1:res=cc;break;case 2:res=n;break;case 3:res=v;break;
   case 4:res=cc&&!z;break;case 5:res=(n==v);break;case 6:res=(n==v)&&!z;break;default:res=1;}
  return ((cond&1)&&cond!=15)? !res:res;
}

/* ── Save-data filesystem ── */

static void mkpath(const char* path) {
  char tmp[512]; size_t len;
  snprintf(tmp,sizeof tmp,"%s",path); len=strlen(tmp);
  for(size_t i=1;i<len;i++){
    if(tmp[i]==PATH_SEP||tmp[i]=='/'){tmp[i]=0; MKDIR(tmp); tmp[i]=PATH_SEP;}
  }
  MKDIR(tmp);
}

int recomp_save_init(GuestContext* c, const char* exe_path) {
  char dir[512];
  /* Put save_data/ next to the executable */
  snprintf(dir,sizeof dir,"%s",exe_path);
  char* sl=strrchr(dir,PATH_SEP);
  if(!sl) sl=strrchr(dir,'/');
  if(sl) *(sl+1)=0; else dir[0]=0;
  snprintf(c->save_dir,sizeof c->save_dir,"%ssave_data",dir);
  mkpath(c->save_dir);
  printf("[recomp] Save directory: %s\n",c->save_dir);
  return 1;
}

int recomp_save_write(GuestContext* c, const char* name, const void* data, uint64_t size) {
  char path[1024];
  snprintf(path,sizeof path,"%s%c%s",c->save_dir,PATH_SEP,name);
  /* Ensure parent dirs exist */
  char parent[1024]; snprintf(parent,sizeof parent,"%s",path);
  char* sl=strrchr(parent,PATH_SEP); if(!sl) sl=strrchr(parent,'/'); if(sl)*sl=0;
  mkpath(parent);
  FILE* f=fopen(path,"wb");
  if(!f){fprintf(stderr,"[recomp] save write failed: %s\n",path); return 0;}
  fwrite(data,1,(size_t)size,f); fclose(f);
  printf("[recomp] Saved %llu bytes -> %s\n",(unsigned long long)size,path);
  return 1;
}

int recomp_save_read(GuestContext* c, const char* name, void* buf, uint64_t buf_size, uint64_t* out_size) {
  char path[1024];
  snprintf(path,sizeof path,"%s%c%s",c->save_dir,PATH_SEP,name);
  FILE* f=fopen(path,"rb");
  if(!f){if(out_size)*out_size=0; return 0;}
  fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
  uint64_t to_read=(uint64_t)sz<buf_size?(uint64_t)sz:buf_size;
  fread(buf,1,(size_t)to_read,f); fclose(f);
  if(out_size)*out_size=to_read;
  printf("[recomp] Loaded %llu bytes <- %s\n",(unsigned long long)to_read,path);
  return 1;
}

int recomp_save_delete(GuestContext* c, const char* name) {
  char path[1024];
  snprintf(path,sizeof path,"%s%c%s",c->save_dir,PATH_SEP,name);
  return remove(path)==0;
}

int recomp_save_exists(GuestContext* c, const char* name) {
  char path[1024];
  snprintf(path,sizeof path,"%s%c%s",c->save_dir,PATH_SEP,name);
  FILE* f=fopen(path,"rb");
  if(f){fclose(f); return 1;} return 0;
}

/* ── Segment loader: loads rodata.bin + data.bin from the data dir into guest memory ── */

int recomp_load_segments(GuestContext* c, const char* data_dir) {
  const char* names[]={"rodata.bin","data.bin","text.bin"};
  /* Corresponding offsets from segment info embedded in manifest — for now, load
     sequentially after .text in memory. The real offsets come from the blockmap. */
  for(int i=0;i<3;i++){
    char path[1024];
    snprintf(path,sizeof path,"%s%c%s",data_dir,PATH_SEP,names[i]);
    FILE* f=fopen(path,"rb");
    if(!f) continue;
    fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
    if((uint64_t)sz<=c->mem_size){
      /* Load at the appropriate offset — text at base, others after */
      fread(c->mem,1,(size_t)sz,f);
    }
    fclose(f);
    printf("[recomp] Loaded segment %s (%ld bytes)\n",names[i],sz);
  }
  return 1;
}

/* ── SVC handler with HLE filesystem support ── */

#ifdef SUYU_HOSTED_RECOMP
/* In the suyu-hosted build the block already set pending_svc before calling
   this. Just return — arm_recomp.cpp's run loop detects pending_svc != ~0 and
   dispatches to the real HLE kernel. */
void recomp_svc(GuestContext* c,unsigned imm){ (void)c; (void)imm; }
#else
void recomp_svc(GuestContext* c,unsigned imm){
  /* Standalone runtime: this handler services the call itself, so clear the
     pending flag. The suyu-hosted build replaces this translation unit with a
     bridge that leaves pending_svc set and returns, handing the call to the
     real HLE kernel instead. */
  c->pending_svc = ~0ULL;
  switch(imm){
  case 0x1: /* SetHeapSize — x1 = requested size */
    if(c->heap_base==0){
      c->heap_base=c->mem_base_vaddr+c->mem_size/2;
      c->heap_cur=c->heap_base;
      c->heap_end=c->heap_base+c->mem_size/2;
    }
    c->x[0]=0; /* success */
    c->x[1]=c->heap_base;
    break;
  case 0x2: /* SetMemoryPermission — stub success */
    c->x[0]=0;
    break;
  case 0x3: /* SetMemoryAttribute — stub success */
    c->x[0]=0;
    break;
  case 0x6: /* QueryMemory — stub: report all memory as readable/writable */
    c->x[0]=0;
    c->x[1]=0; /* MemoryInfo written to [x0] — simplified */
    break;
  case 0x7: /* ExitProcess */
    printf("[recomp] ExitProcess called\n");
    c->halted=1;
    break;
  case 0x8: /* CreateThread — stub, return handle=1 */
    c->x[0]=0; c->x[1]=1;
    break;
  case 0xB: /* SleepThread — yield CPU to prevent spin-lock lag */
#ifdef _WIN32
    Sleep((DWORD)(c->x[0] / 1000000ULL)); /* ns to ms */
#else
    { struct timespec ts; ts.tv_sec=0; ts.tv_nsec=(long)(c->x[0]>0?c->x[0]:1000000);
      nanosleep(&ts,0); }
#endif
    c->x[0]=0;
    break;
  case 0x15: /* SendSyncRequest — IPC for fsp-srv / save data */
    /* Simplified HLE: check x[0] for handle, interpret IPC command buffer.
       For now, stub success so game save code paths don't crash. */
    c->x[0]=0;
    break;
  case 0x16: /* SendSyncRequestWithUserBuffer */
    c->x[0]=0;
    break;
  case 0x18: /* CloseHandle — stub */
    c->x[0]=0;
    break;
  case 0x1A: /* WaitSynchronization — stub immediate return */
    c->x[0]=0; c->x[1]=0;
    break;
  case 0x1F: /* ConnectToNamedPort — stub, return handle */
    c->x[0]=0; c->x[1]=0x100;
    break;
  case 0x21: /* SendSyncRequest (sm: variant) */
    c->x[0]=0;
    break;
  case 0x26: /* Break — debug break */
    printf("[recomp] Break SVC x0=%llu\n",(unsigned long long)c->x[0]);
    break;
  case 0x27: /* OutputDebugString */
    { uint8_t* p=memptr(c,c->x[0],(uint64_t)c->x[1]);
      if(p) printf("[guest] %.*s\n",(int)c->x[1],(char*)p);
      c->x[0]=0;
    }
    break;
  case 0x29: /* GetInfo — return stub values for system info queries */
    { uint32_t id=(uint32_t)c->x[1];
      switch(id){
      case 0: c->x[1]=0xFFFFFF; break; /* AllowedCPUCoreMask */
      case 1: c->x[1]=0xF; break; /* AllowedThreadPrioMask */
      case 2: c->x[1]=c->mem_base_vaddr; break; /* MapRegionBaseAddr */
      case 3: c->x[1]=c->mem_size; break; /* MapRegionSize */
      case 4: c->x[1]=c->heap_base; break; /* HeapRegionBaseAddr */
      case 5: c->x[1]=c->heap_end-c->heap_base; break; /* HeapRegionSize */
      case 6: c->x[1]=c->mem_size; break; /* TotalMemorySize */
      case 7: c->x[1]=c->mem_size/2; break; /* UsedMemorySize */
      case 12: c->x[1]=c->mem_base_vaddr+c->mem_size; break; /* AslrRegionBaseAddr */
      case 13: c->x[1]=0x1000000; break; /* AslrRegionSize */
      case 14: c->x[1]=c->mem_base_vaddr+c->mem_size; break; /* StackRegionBaseAddr */
      case 15: c->x[1]=0x100000; break; /* StackRegionSize */
      default: c->x[1]=0; break;
      }
      c->x[0]=0;
    }
    break;
  default:
    printf("[recomp] Unhandled SVC #0x%x x0=0x%llx x1=0x%llx\n",imm,
      (unsigned long long)c->x[0],(unsigned long long)c->x[1]);
    c->x[0]=0;
    break;
  }
}
#endif /* SUYU_HOSTED_RECOMP */

/* An opcode the decoder does not implement. Stubbing it out (the old
   behaviour: zero x0 and carry on) silently corrupts guest state - a
   function whose body is one unimplemented SIMD instruction returns a
   plausible-looking 0, and the caller then dereferences it. That is
   indistinguishable from a real null and shows up much later as a crash
   nowhere near the actual gap.

   Instead, park the context at this exact PC and halt with a distinct code.
   The host (ArmRecomp::RunThread) treats halted == RECOMP_HALT_UNHANDLED as
   "run this address on the interpreter fallback instead", so the instruction
   executes correctly and control returns to recompiled code as soon as the
   PC is covered again. Correct by construction whatever the decoder does or
   does not cover, and every remaining gap costs speed rather than
   correctness.

   Standalone builds have no fallback engine to hand off to, so they keep the
   old step-over behaviour - degraded, but still the best available there. */
/* DMB/DSB/ISB. Under the hosted backend the guest is genuinely multi-threaded,
   so these must be real fences: without one the host compiler is free to sink a
   pointer store past the field stores the barrier was there to publish. The
   standalone runtime drives one guest thread on one host thread and has nothing
   to order against, so it keeps the free no-op. */
void recomp_barrier(void){
#ifdef SUYU_HOSTED_RECOMP
#if defined(__cplusplus)
    atomic_thread_fence(memory_order_seq_cst);
#elif defined(_MSC_VER)
    MemoryBarrier();
#else
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
#endif
#endif
}

void recomp_unhandled(GuestContext* c,uint32_t insn,uint64_t pc){
#ifdef SUYU_HOSTED_RECOMP
  (void)insn;
  c->pc=pc;
  c->halted=RECOMP_HALT_UNHANDLED;
#else
  fprintf(stderr,"[recomp] unhandled insn 0x%08x at 0x%llx\n",insn,(unsigned long long)pc);
  (void)c; /* stepping over is bad enough; do not clobber a register too */
#endif
}

#ifndef RECOMP_STATIC_HOST
/* Drives a single module's own dispatch table. Meaningless when the runtime is
   shared between several statically linked modules - the host dispatches
   across them instead - so it is compiled out there, where recomp_lookup has
   been renamed per module and would not resolve. */
void recomp_run(GuestContext* c){
  uint64_t g=0;
  while(!c->halted){
    BlockFn f=recomp_lookup(c->pc);
    if(!f){ fprintf(stderr,"[recomp] no block at 0x%llx\n",(unsigned long long)c->pc); break;}
    f(c);
    if(++g>100000000ULL){ fprintf(stderr,"[recomp] watchdog (100M iterations)\n"); break; }
    /* Yield every 4096 blocks to prevent 100% CPU spin on tight loops */
    if((g & 0xFFF)==0){
#ifdef _WIN32
      Sleep(0);
#else
      { struct timespec ts={0,0}; nanosleep(&ts,0); }
#endif
    }
  }
}
#endif /* !RECOMP_STATIC_HOST */
)RT";
}

} // namespace suyu::recomp
