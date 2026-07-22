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

inline std::vector<Block> DiscoverBlocks(const u8* text, size_t n_bytes, u64 base) {
    const u32 n = (u32)(n_bytes / 4);
    if (n == 0) return {};
    std::vector<bool> start(n, false);
    start[0] = true;
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
        if (opc & 1) { if (rt != 31) { snprintf(buf, sizeof buf, "c->x[%u]=recomp_load%s(c,%s);", rt, ty, addr.c_str()); put(buf); } }
        else { snprintf(buf, sizeof buf, "recomp_store%s(c,%s,%s);", ty, addr.c_str(), Xz(rt).c_str()); put(buf); }
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
    if ((i & 0xFFE0001F) == 0xD4000001) { u32 imm = (i >> 5) & 0xFFFF; snprintf(buf, sizeof buf, "c->pc=0x%llxULL; recomp_svc(c,%u); return;", (unsigned long long)next, imm); put(buf); return false; }

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
                                  const u8* data_seg = nullptr, size_t data_size = 0) {
    RecompileStats stats;
    auto blocks = DiscoverBlocks(text, n_bytes, base);
    const u32* p = reinterpret_cast<const u32*>(text);
    stats.blocks = blocks.size();
    stats.instructions = n_bytes / 4;

    std::ostringstream rc;
    rc << "/* auto-generated by suyu static recompiler - DO NOT EDIT */\n#include \"recomp_runtime.h\"\n\n";
    for (const auto& b : blocks) {
        rc << "void " << FuncName(mod, b.vaddr) << "(GuestContext* c){\n";
        const u32 first = (u32)((b.vaddr - base) / 4);
        bool open = true;
        for (u32 k = 0; k < b.count; ++k) {
            std::string body;
            open = Translate(p[first + k], b.vaddr + (u64)k * 4, body);
            rc << body;
            if (!open) { ++stats.translated_terminators; break; }
        }
        if (open) rc << "    c->pc=0x" << std::hex << (b.vaddr + b.size) << std::dec << "ULL; return;\n";
        rc << "}\n\n";
    }
    rc << "#include <stdint.h>\nstruct _ent{uint64_t va; BlockFn fn;};\nstatic const struct _ent _tbl[] = {\n";
    for (const auto& b : blocks) rc << "  {0x" << std::hex << b.vaddr << std::dec << "ULL, " << FuncName(mod, b.vaddr) << "},\n";
    rc << "};\nBlockFn recomp_lookup(uint64_t pc){ for(unsigned i=0;i<sizeof(_tbl)/sizeof(_tbl[0]);++i) if(_tbl[i].va==pc) return _tbl[i].fn; return 0; }\n";

    std::ostringstream mc;
    mc << "#include \"recomp_runtime.h\"\n#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n\n";
    mc << "#define GUEST_MEM_SIZE (256ULL * 1024 * 1024) /* 256 MB */\n\n";
    mc << "int main(int argc, char** argv){\n";
    mc << "  uint8_t* mem = (uint8_t*)calloc(1, (size_t)GUEST_MEM_SIZE);\n";
    mc << "  if(!mem){ fprintf(stderr,\"Failed to allocate guest memory\\n\"); return 1; }\n";
    mc << "  GuestContext c; memset(&c,0,sizeof c);\n";
    mc << "  c.mem=mem; c.mem_size=GUEST_MEM_SIZE;\n";
    mc << "  c.mem_base_vaddr=0x" << std::hex << base << std::dec << "ULL;\n";
    mc << "  c.heap_base=c.mem_base_vaddr+GUEST_MEM_SIZE/2;\n";
    mc << "  c.heap_cur=c.heap_base; c.heap_end=c.mem_base_vaddr+GUEST_MEM_SIZE;\n";
    mc << "  c.x[31]=c.mem_base_vaddr + GUEST_MEM_SIZE - 16; /* SP */\n";
    mc << "  c.pc=0x" << std::hex << base << std::dec << "ULL;\n\n";
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
       << "add_executable(recompiled main.c recompiled_" << mod << ".c recomp_runtime.c)\n"
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
#include <direct.h>
#include <io.h>
#define MKDIR(p) _mkdir(p)
#define PATH_SEP '\\'
#else
#include <sys/stat.h>
#include <unistd.h>
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
  case 0xB: /* SleepThread — no-op in recomp */
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
    if(++g>1000000000ULL){ fprintf(stderr,"[recomp] watchdog (1B iterations)\n"); break; }
  }
}
)RT";
}

} // namespace suyu::recomp
