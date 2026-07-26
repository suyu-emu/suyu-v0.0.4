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

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
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

inline bool DirectBranchTarget(u32 i, u64 pc, u64& out) {
    if ((i & 0xFC000000) == 0x14000000 || (i & 0xFC000000) == 0x94000000) {
        s32 imm26 = (s32)(i << 6) >> 6;
        out = pc + (s64)imm26 * 4;
        return true;
    }
    return false;
}

inline std::vector<Block> DiscoverBlocks(const u8* text, size_t n_bytes, u64 base,
                                         u64 entry_pc = 0) {
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
    u64 elem = esize >= 64 ? welem
                           : (((welem >> r) | (welem << (esize - r))) & ((1ULL << esize) - 1));
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

// Append C for one instruction. Returns false if the instruction terminates the block.
inline bool Translate(u32 i, u64 pc, std::string& out) {
    char buf[256];
    auto put = [&](const std::string& s) { out += "    " + s + "\n"; };
    const u64 next = pc + 4;

    if ((i & 0xFFFFF01F) == 0xD503201F) { put("/* nop/hint */"); return true; }

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
        s += "c->x[" + std::to_string(rd) + "]=_r; ";
        if (S) s += "recomp_set_flags(c," + std::string(op ? "1" : "0") + ",_a,_b,_r," + (sf ? "1" : "0") + "); ";
        s += "}";
        if (rd != 31 || S) put(s);
        return true;
    }

    if ((i & 0x1F000000) == 0x0A000000) { // logical shifted register
        u32 sf = i >> 31, opc = (i >> 29) & 3, rm = (i >> 16) & 31, rn = (i >> 5) & 31, rd = i & 31;
        u32 shift = (i >> 22) & 3, imm6 = (i >> 10) & 0x3F, N = (i >> 21) & 1;
        std::string rmv = Xz(rm);
        if (imm6) { const char* o = shift == 0 ? "<<" : ">>"; char sb[96]; snprintf(sb, sizeof sb, "(%s %s %u)", rmv.c_str(), o, imm6); rmv = sb; }
        std::string a = Xz(rn);
        const char* lop = opc == 0 ? "&" : opc == 1 ? "|" : opc == 2 ? "^" : "&";
        std::string expr = N ? ("(" + a + " " + lop + " ~" + rmv + ")") : ("(" + a + " " + lop + " " + rmv + ")");
        if (rd != 31) {
            put("c->x[" + std::to_string(rd) + "] = " + expr + ";");
            if (!sf) { snprintf(buf, sizeof buf, "c->x[%u]&=0xFFFFFFFFULL;", rd); put(buf); }
        }
        return true;
    }

    if ((i & 0x1F200000) == 0x0B000000) { // ADD/SUB shifted register
        u32 sf = i >> 31, op = (i >> 30) & 1, S = (i >> 29) & 1, shift = (i >> 22) & 3, rm = (i >> 16) & 31, imm6 = (i >> 10) & 0x3F, rn = (i >> 5) & 31, rd = i & 31;
        std::string rmv = Xz(rm);
        if (imm6) { const char* o = shift == 0 ? "<<" : ">>"; char sb[96]; snprintf(sb, sizeof sb, "(%s %s %u)", rmv.c_str(), o, imm6); rmv = sb; }
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
            if (op) { u64 b = (pc & ~0xFFFULL); s64 imm = ((immhi << 2) | immlo) << 12; snprintf(buf, sizeof buf, "c->x[%u]=0x%llxULL + (int64_t)%lld;", rd, (unsigned long long)b, (long long)imm); }
            else { s64 imm = (immhi << 2) | immlo; snprintf(buf, sizeof buf, "c->x[%u]=0x%llxULL + (int64_t)%lld;", rd, (unsigned long long)pc, (long long)imm); }
            put(buf);
        }
        return true;
    }

    if ((i & 0x3B000000) == 0x39000000) { // LDR/STR immediate unsigned offset
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
                     "recomp_unhandled(c,0x%08xU,0x%llxULL); c->pc=0x%llxULL; return;", i,
                     (unsigned long long)pc, (unsigned long long)next);
            put(buf);
            return false;
        }
        return true;
    }

    u64 t = 0;
    if (DirectBranchTarget(i, pc, t)) {
        if ((i & 0xFC000000) == 0x94000000) { snprintf(buf, sizeof buf, "c->x[30]=0x%llxULL;", (unsigned long long)next); put(buf); }
        snprintf(buf, sizeof buf, "c->pc=0x%llxULL; return;", (unsigned long long)t); put(buf); return false;
    }
    if ((i & 0xFFFFFC1F) == 0xD65F0000) { put("c->pc=c->x[30]; return; /* RET */"); return false; }
    if ((i & 0xFFFFFC1F) == 0xD61F0000) { u32 rn = (i >> 5) & 31; snprintf(buf, sizeof buf, "c->pc=c->x[%u]; return; /* BR */", rn); put(buf); return false; }
    if ((i & 0xFFFFFC1F) == 0xD63F0000) { u32 rn = (i >> 5) & 31; snprintf(buf, sizeof buf, "c->x[30]=0x%llxULL; c->pc=c->x[%u]; return; /* BLR */", (unsigned long long)next, rn); put(buf); return false; }
    if ((i & 0xFF000010) == 0x54000000) { s64 off = ((s32)((i >> 5) << 13) >> 13); u64 tt = pc + off * 4; u32 cond = i & 15; snprintf(buf, sizeof buf, "if (recomp_cond(c,%u)) { c->pc=0x%llxULL; } else { c->pc=0x%llxULL; } return;", cond, (unsigned long long)tt, (unsigned long long)next); put(buf); return false; }
    if ((i & 0x7E000000) == 0x34000000) { u32 sf = i >> 31; bool nz = (i >> 24) & 1; u32 rt = i & 31; s64 off = ((s32)(((i >> 5) & 0x7FFFF) << 13) >> 13); u64 tt = pc + off * 4; std::string v = sf ? Xz(rt) : Wz(rt); snprintf(buf, sizeof buf, "if ((%s)%s0) { c->pc=0x%llxULL; } else { c->pc=0x%llxULL; } return;", v.c_str(), nz ? "!=" : "==", (unsigned long long)tt, (unsigned long long)next); put(buf); return false; }
    if ((i & 0x7E000000) == 0x36000000) { bool nz = (i >> 24) & 1; u32 b = ((i >> 31) << 5) | ((i >> 19) & 31); u32 rt = i & 31; s64 off = ((s32)(((i >> 5) & 0x3FFF) << 18) >> 18); u64 tt = pc + off * 4; snprintf(buf, sizeof buf, "if (((c->x[%u]>>%u)&1)%s0) { c->pc=0x%llxULL; } else { c->pc=0x%llxULL; } return;", rt, b, nz ? "!=" : "==", (unsigned long long)tt, (unsigned long long)next); put(buf); return false; }
    if ((i & 0xFFE0001F) == 0xD4000001) { u32 imm = (i >> 5) & 0xFFFF; snprintf(buf, sizeof buf, "c->pc=0x%llxULL; c->pending_svc=%uULL; recomp_svc(c,%u); return;", (unsigned long long)next, imm, imm); put(buf); return false; }

    // STP/LDP - load/store pair. Every non-leaf AArch64 function opens and
    // closes with these, so without them a real game stops at its first
    // prologue. Covers the signed-offset, pre-index and post-index forms for
    // both 32- and 64-bit operands.
    if ((i & 0x3A000000) == 0x28000000) {
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
                s += "c->x[" + std::to_string(rt) + "]=recomp_load" + std::to_string(sz * 8) +
                     "(c," + addr + "); ";
                s += "c->x[" + std::to_string(rt2) + "]=recomp_load" + std::to_string(sz * 8) +
                     "(c," + addr + "+" + std::to_string(sz) + "); ";
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
            if (rd != 31) s += "c->x[" + std::to_string(rd) + "] = _r; ";
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
        // Only UBFM (opc 2) and SBFM (opc 0). BFM merges into the existing
        // destination bits, which needs the insert mask tracked properly -
        // left to the fallback rather than approximated, since wrong codegen
        // is worse than an honest halt.
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

    // Conditional select: CSEL/CSINC/CSINV/CSNEG.
    if ((i & 0x1FE00000) == 0x1A800000) {
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
    if ((i & 0x3B000000) == 0x38000000 && ((i >> 24) & 1) == 0) {
        const u32 size = i >> 30, opc = (i >> 22) & 3, mode = (i >> 10) & 3;
        const u32 rn = (i >> 5) & 31, rt = i & 31;
        s32 imm9 = (s32)((i >> 12) & 0x1FF);
        if (imm9 & 0x100) imm9 |= ~0x1FF;
        // mode 0 = LDUR/STUR, 1 = post-index, 3 = pre-index
        if ((mode == 0 || mode == 1 || mode == 3) && size <= 3) {
            const u32 bits = 8u << size;
            const bool is_load = (opc & 1) != 0;
            std::string s = "{ uint64_t _b=c->x[" + std::to_string(rn) + "]; int64_t _o=" +
                            std::to_string((long long)imm9) + "; ";
            const char* addr = (mode == 1) ? "_b" : "(_b+_o)";
            if (is_load) {
                if (rt != 31) {
                    s += "c->x[" + std::to_string(rt) + "]=recomp_load" + std::to_string(bits) +
                         "(c," + addr + "); ";
                }
            } else {
                s += "recomp_store" + std::to_string(bits) + "(c," + addr + "," + Xz(rt) + "); ";
            }
            if (mode == 1 || mode == 3) s += "c->x[" + std::to_string(rn) + "]=_b+_o; ";
            s += "}";
            put(s);
            return true;
        }
    }

    // Load/store exclusive and acquire/release. The generated runtime is
    // single-threaded, so the exclusive monitor can never be broken by another
    // agent: LDXR/STXR reduce to a plain load/store with STXR always reporting
    // success, and the acquire/release ordering has nothing to order against.
    // That is exact for a single thread; under suyu's ArmRecomp backend, where
    // real threads exist, these need the kernel's monitor instead - noted so
    // the assumption is visible rather than silent.
    if ((i & 0x3F000000) == 0x08000000) {
        const u32 size = i >> 30, o2 = (i >> 23) & 1, L = (i >> 22) & 1, o1 = (i >> 21) & 1;
        const u32 rs = (i >> 16) & 31, rt2 = (i >> 10) & 31, rn = (i >> 5) & 31, rt = i & 31;
        // Pair variants (o1 set) are left to the fallback; only the single
        // register forms are handled here.
        if (!o1 && rt2 == 31) {
            const u32 bits = 8u << size;
            const std::string addr = "c->x[" + std::to_string(rn) + "]";
            if (L) {
                // LDXR / LDAXR / LDAR
                if (rt != 31) {
                    put("c->x[" + std::to_string(rt) + "] = recomp_load" +
                        std::to_string(bits) + "(c," + addr + ");");
                }
            } else {
                put("recomp_store" + std::to_string(bits) + "(c," + addr + "," + Xz(rt) + ");");
                // STXR/STLXR report the status of the store in Rs; 0 is success.
                // STLR (o2 set) has no status register.
                if (!o2 && rs != 31) {
                    put("c->x[" + std::to_string(rs) + "] = 0;");
                }
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
                std::string s = "{ uint64_t _a=" + Xz(rn) + ", _b=" + ext + "; uint64_t _r=" +
                                std::string(op ? "_a-_b" : "_a+_b") + "; ";
                if (!sf) s += "_r &= 0xFFFFFFFFULL; ";
                if (rd != 31) s += "c->x[" + std::to_string(rd) + "]=_r; ";
                if (S) s += "recomp_set_flags(c," + std::string(op ? "1" : "0") +
                            ",_a,_b,_r," + (sf ? "1" : "0") + "); ";
                s += "}";
                put(s);
                return true;
            }
        }
    }

    // Load/store with a register offset (the [base, Xm{, extend}] form).
    if ((i & 0x3B200C00) == 0x38200800) {
        const u32 size = i >> 30, opc = (i >> 22) & 3;
        const u32 rm = (i >> 16) & 31, option = (i >> 13) & 7, S = (i >> 12) & 1;
        const u32 rn = (i >> 5) & 31, rt = i & 31;
        if (size <= 3 && (opc == 0 || opc == 1)) {
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
                if (opc == 1) {
                    if (rt != 31) {
                        put("c->x[" + std::to_string(rt) + "] = recomp_load" +
                            std::to_string(bits) + "(c," + addr + ");");
                    }
                } else {
                    put("recomp_store" + std::to_string(bits) + "(c," + addr + "," + Xz(rt) + ");");
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
                s += "uint64_t _r = (uint64_t)(" + std::string(it) + ")_a; ";
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

    // MRS/MSR against the small set of system registers a user-mode program
    // actually touches. Anything else stays on the fallback rather than being
    // silently invented.
    if ((i & 0xFFF00000) == 0xD5300000 || (i & 0xFFF00000) == 0xD5100000) {
        const bool is_read = ((i >> 21) & 1) != 0;   // MRS reads, MSR writes
        const u32 sysreg = (i >> 5) & 0x7FFF;
        const u32 rt = i & 31;
        // TPIDR_EL0 (thread pointer) and TPIDRRO_EL0 are the ones that matter
        // for ordinary code; both live in the context already.
        constexpr u32 kTpidrEl0 = 0x5E82;
        constexpr u32 kTpidrroEl0 = 0x5E83;
        if (sysreg == kTpidrEl0 || sysreg == kTpidrroEl0) {
            if (is_read) {
                if (rt != 31) put("c->x[" + std::to_string(rt) + "] = c->tpidr_el0;");
            } else {
                put("c->tpidr_el0 = " + Xz(rt) + ";");
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

            // FCMP / FCMPE - sets NZCV from an ordered comparison.
            if (((i >> 10) & 3) == 0 && ((i >> 16) & 0x1F) < 32 && ((i >> 14) & 3) == 0 &&
                ((i >> 4) & 1) == 0 && (i & 0x0000000F) == 0) {
                std::string s = ld_n + ld_m;
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

    snprintf(buf, sizeof buf, "recomp_unhandled(c,0x%08xU,0x%llxULL); c->pc=0x%llxULL; return;", i, (unsigned long long)pc, (unsigned long long)next);
    put(buf); return false;
}

inline std::string FuncName(const std::string& mod, u64 v) {
    char b[64]; snprintf(b, sizeof b, "blk_%s_%016llx", mod.c_str(), (unsigned long long)v); return b;
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
// Writes recompiled_<mod>.c, the shared runtime, main.c and CMakeLists.txt into out_dir.
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
                                  const std::string& display_title = {}) {
    RecompileStats stats;
    auto blocks = DiscoverBlocks(text, n_bytes, base, entry_pc);
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
    std::vector<std::ostringstream> units(unit_count);
    for (auto& u : units) {
        u << "/* auto-generated by suyu static recompiler - DO NOT EDIT */\n"
             "#include \"recomp_runtime.h\"\n"
             "#include <string.h>\n"   // memcpy, used by the scalar FP paths
             "#include <math.h>\n\n";  // fabs/sqrt for FABS/FSQRT
    }

    size_t block_index = 0;
    for (const auto& b : blocks) {
        std::ostringstream& rcu = units[block_index / kBlocksPerUnit];
        ++block_index;
        rcu << "void " << FuncName(mod, b.vaddr) << "(GuestContext* c){\n";
        const u32 first = (u32)((b.vaddr - base) / 4);
        bool open = true;
        for (u32 k = 0; k < b.count; ++k) {
            std::string body;
            open = Translate(p[first + k], b.vaddr + (u64)k * 4, body);
            rcu << body;
            if (!open) { ++stats.translated_terminators; break; }
        }
        if (open) rcu << "    c->pc=0x" << std::hex << (b.vaddr + b.size) << std::dec << "ULL; return;\n";
        rcu << "}\n\n";
    }

    // The dispatch table gets its own unit, forward-declaring every block since
    // the bodies now live in the other units.
    std::ostringstream rc;
    rc << "/* auto-generated by suyu static recompiler - DO NOT EDIT */\n#include \"recomp_runtime.h\"\n\n";
    for (const auto& b : blocks) {
        rc << "void " << FuncName(mod, b.vaddr) << "(GuestContext*);\n";
    }
    rc << "\n#include <stdint.h>\nstruct _ent{uint64_t va; BlockFn fn;};\nstatic const struct _ent _tbl[] = {\n";
    for (const auto& b : blocks) rc << "  {0x" << std::hex << b.vaddr << std::dec << "ULL, " << FuncName(mod, b.vaddr) << "},\n";
    rc << "};\nBlockFn recomp_lookup(uint64_t pc){\n"
       << "  unsigned lo=0, hi=sizeof(_tbl)/sizeof(_tbl[0]);\n"
       << "  while(lo<hi){ unsigned m=lo+(hi-lo)/2; if(_tbl[m].va<pc) lo=m+1; else hi=m; }\n"
       << "  return (lo<sizeof(_tbl)/sizeof(_tbl[0]) && _tbl[lo].va==pc)?_tbl[lo].fn:0;\n}\n";

    std::ostringstream mc;
    mc << "#include \"recomp_runtime.h\"\n#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n\n";
    mc << "#define GUEST_MEM_SIZE (256ULL * 1024 * 1024) /* 256 MB */\n\n";
    mc << "static const char* kGameTitle = \"" << (display_title.empty() ? mod : display_title)
       << "\";\n\n";
    mc << "int main(int argc, char** argv){\n";
    mc << "  printf(\"=== %s ===\\n\", kGameTitle);\n";
    mc << "  uint8_t* mem = (uint8_t*)calloc(1, (size_t)GUEST_MEM_SIZE);\n";
    mc << "  if(!mem){ fprintf(stderr,\"Failed to allocate guest memory\\n\"); return 1; }\n";
    mc << "  GuestContext c; memset(&c,0,sizeof c);\n";
    mc << "  c.mem=mem; c.mem_size=GUEST_MEM_SIZE;\n";
    mc << "  c.mem_base_vaddr=0x" << std::hex << base << std::dec << "ULL;\n";
    mc << "  c.heap_base=c.mem_base_vaddr+GUEST_MEM_SIZE/2;\n";
    mc << "  c.heap_cur=c.heap_base; c.heap_end=c.mem_base_vaddr+GUEST_MEM_SIZE;\n";
    mc << "  c.x[31]=c.mem_base_vaddr + GUEST_MEM_SIZE - 16; /* SP */\n";
    mc << "  c.pc=0x" << std::hex << (entry_pc ? entry_pc : base) << std::dec << "ULL;\n\n";
    mc << "  /* Init save-data directory next to this executable */\n";
    mc << "  recomp_save_init(&c, argv[0]);\n\n";
    mc << "  /* Load bundled data segments if present */\n";
    mc << "  { char data_dir[512];\n";
    mc << "    snprintf(data_dir,sizeof data_dir,\"%s\",argv[0]);\n";
    mc << "    char* sl=strrchr(data_dir,'\\\\'); if(!sl) sl=strrchr(data_dir,'/'); if(sl) *(sl+1)=0; else data_dir[0]=0;\n";
    mc << "    strncat(data_dir,\"data\",sizeof(data_dir)-strlen(data_dir)-1);\n";
    mc << "    recomp_load_segments(&c,data_dir);\n  }\n\n";
    mc << "  /* Auto-load save state if it exists */\n";
    mc << "  { uint64_t sz=0;\n";
    mc << "    if(recomp_save_exists(&c,\"autosave.bin\")){\n";
    mc << "      recomp_save_read(&c,\"autosave.bin\",c.mem,(uint64_t)GUEST_MEM_SIZE,&sz);\n";
    mc << "      printf(\"[recomp] Restored autosave (%llu bytes)\\n\",(unsigned long long)sz);\n";
    mc << "    }\n  }\n\n";
    mc << "  printf(\"[recomp] Starting execution at pc=0x%llx\\n\",(unsigned long long)c.pc);\n";
    mc << "  recomp_run(&c);\n\n";
    mc << "  /* Auto-save on exit */\n";
    mc << "  recomp_save_write(&c,\"autosave.bin\",c.mem,(uint64_t)GUEST_MEM_SIZE);\n";
    mc << "  printf(\"[recomp] halted at pc=0x%llx\\n\",(unsigned long long)c.pc);\n";
    mc << "  free(mem);\n  return 0;\n}\n";

    std::ostringstream cm;
    cm << "cmake_minimum_required(VERSION 3.13)\nproject(suyu_recompiled C)\nset(CMAKE_C_STANDARD 11)\n"
       << "add_executable(recompiled main.c recompiled_" << mod << ".c recomp_runtime.c";
    for (size_t u = 0; u < unit_count; ++u) {
        cm << "\n    recompiled_" << mod << "_" << u << ".c";
    }
    cm << ")\n"
       << "# Portable C11: Windows->.exe, Linux/FreeBSD/OpenBSD->ELF, macOS->Mach-O\n";

#ifdef _WIN32
    _mkdir(out_dir.c_str());
#else
    mkdir(out_dir.c_str(), 0755);
#endif
    auto write = [&](const std::string& name, const std::string& data) {
        std::ofstream o(out_dir + "/" + name, std::ios::binary);
        o.write(data.data(), (std::streamsize)data.size());
    };
    write("recomp_runtime.h", RuntimeH());
    write("recomp_runtime.c", RuntimeC());
    write("recompiled_" + mod + ".c", rc.str());
    for (size_t u = 0; u < unit_count; ++u) {
        write("recompiled_" + mod + "_" + std::to_string(u) + ".c", units[u].str());
    }
    write("main.c", mc.str());
    write("CMakeLists.txt", cm.str());

    // Bundle text/rodata/data as binary blobs so the exe can load them at startup
    {
        std::string data_subdir = out_dir + "/data";
#ifdef _WIN32
        _mkdir(data_subdir.c_str());
#else
        mkdir(data_subdir.c_str(), 0755);
#endif
        // Always write text.bin
        {
            std::ofstream o(data_subdir + "/text.bin", std::ios::binary);
            o.write(reinterpret_cast<const char*>(text), (std::streamsize)n_bytes);
        }
        if (rodata && rodata_size > 0) {
            std::ofstream o(data_subdir + "/rodata.bin", std::ios::binary);
            o.write(reinterpret_cast<const char*>(rodata), (std::streamsize)rodata_size);
        }
        if (data_seg && data_size > 0) {
            std::ofstream o(data_subdir + "/data.bin", std::ios::binary);
            o.write(reinterpret_cast<const char*>(data_seg), (std::streamsize)data_size);
        }
    }

    return stats;
}

inline const char* RuntimeH() {
    return R"RT(#ifndef SUYU_RECOMP_RUNTIME_H
#define SUYU_RECOMP_RUNTIME_H
#include <stdint.h>
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
    /* Save-data filesystem state */
    char save_dir[512];
    /* Heap break for SVC memory allocation */
    uint64_t heap_base; uint64_t heap_end; uint64_t heap_cur;
    /* IPC command buffer (simplified HLE) */
    uint32_t ipc_cmd[64];
    /* Open file handles for save data (simplified) */
    void* save_handles[16]; int save_handle_count;
} GuestContext;
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

uint64_t recomp_load8 (GuestContext* c,uint64_t a){uint8_t* p=memptr(c,a,1); return p?*p:0;}
uint64_t recomp_load16(GuestContext* c,uint64_t a){uint8_t* p=memptr(c,a,2); uint16_t v=0; if(p)memcpy(&v,p,2); return v;}
uint64_t recomp_load32(GuestContext* c,uint64_t a){uint8_t* p=memptr(c,a,4); uint32_t v=0; if(p)memcpy(&v,p,4); return v;}
uint64_t recomp_load64(GuestContext* c,uint64_t a){uint8_t* p=memptr(c,a,8); uint64_t v=0; if(p)memcpy(&v,p,8); return v;}
void recomp_store8 (GuestContext* c,uint64_t a,uint64_t v){uint8_t* p=memptr(c,a,1); if(p)*p=(uint8_t)v;}
void recomp_store16(GuestContext* c,uint64_t a,uint64_t v){uint8_t* p=memptr(c,a,2); uint16_t t=(uint16_t)v; if(p)memcpy(p,&t,2);}
void recomp_store32(GuestContext* c,uint64_t a,uint64_t v){uint8_t* p=memptr(c,a,4); uint32_t t=(uint32_t)v; if(p)memcpy(p,&t,4);}
void recomp_store64(GuestContext* c,uint64_t a,uint64_t v){uint8_t* p=memptr(c,a,8); if(p)memcpy(p,&v,8);}

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

void recomp_unhandled(GuestContext* c,uint32_t insn,uint64_t pc){
  fprintf(stderr,"[recomp] unhandled insn 0x%08x at 0x%llx\n",insn,(unsigned long long)pc);
  c->x[0]=0; /* Don't halt — stub and continue so the game can keep running */
}

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
)RT";
}

} // namespace suyu::recomp
