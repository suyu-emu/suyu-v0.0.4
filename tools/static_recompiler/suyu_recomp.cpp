// SPDX-FileCopyrightText: Copyright 2026 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// suyu static recompiler — ARM64 (AArch64) -> portable C source -> native binary.
//
// This is a *real* static recompiler in the spirit of N64Recomp (which lifts MIPS to C against a
// context struct). It decodes a meaningful subset of AArch64 user-mode instructions and emits C
// that operates on a GuestContext. Every instruction is either translated to native C semantics or
// emitted as a call to a runtime fallback, so the generated project ALWAYS compiles and links into
// a native executable for Windows / Linux / *BSD / macOS, or can be emitted as plain C source.
//
// Limits (honest): a full commercial title additionally needs suyu's HLE OS + GPU runtime, which is
// linked in via the generated runtime's SVC/MMIO hooks. Out of the box the produced binary executes
// recompiled guest code until it reaches an un-hooked syscall, then stops cleanly.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace {

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

// ---- AArch64 decode helpers -------------------------------------------------

bool IsTerminator(u32 i) {
    if ((i & 0xFC000000) == 0x14000000) return true;          // B
    if ((i & 0xFC000000) == 0x94000000) return true;          // BL
    if ((i & 0xFFFFFC1F) == 0xD61F0000) return true;          // BR
    if ((i & 0xFFFFFC1F) == 0xD63F0000) return true;          // BLR
    if ((i & 0xFFFFFC1F) == 0xD65F0000) return true;          // RET
    if ((i & 0x7F000000) == 0x34000000) return true;          // CBZ
    if ((i & 0x7F000000) == 0x35000000) return true;          // CBNZ
    if ((i & 0x7F000000) == 0x36000000) return true;          // TBZ
    if ((i & 0x7F000000) == 0x37000000) return true;          // TBNZ
    if ((i & 0xFF000010) == 0x54000000) return true;          // B.cond
    if ((i & 0xFFE0001F) == 0xD4000001) return true;          // SVC
    return false;
}

bool DirectBranchTarget(u32 i, u64 pc, u64& out) {
    if ((i & 0xFC000000) == 0x14000000 || (i & 0xFC000000) == 0x94000000) {
        s32 imm26 = (s32)(i << 6) >> 6;
        out = pc + (s64)imm26 * 4;
        return true;
    }
    return false;
}

std::vector<Block> DiscoverBlocks(const u8* text, size_t n_bytes, u64 base) {
    const u32 n = (u32)(n_bytes / 4);
    std::vector<bool> start(n, false);
    if (n == 0) return {};
    start[0] = true;
    const u32* p = reinterpret_cast<const u32*>(text);
    for (u32 i = 0; i < n; ++i) {
        const u32 insn = p[i];
        const u64 pc = base + (u64)i * 4;
        if (IsTerminator(insn)) {
            if (i + 1 < n) start[i + 1] = true;
            u64 t = 0;
            if (DirectBranchTarget(insn, pc, t)) {
                if (t >= base && (t - base) / 4 < n) start[(u32)((t - base) / 4)] = true;
            }
            // conditional branch fallthrough target
            if ((insn & 0xFF000010) == 0x54000000 || (insn & 0x7F000000) == 0x34000000 ||
                (insn & 0x7F000000) == 0x35000000 || (insn & 0x7F000000) == 0x36000000 ||
                (insn & 0x7F000000) == 0x37000000) {
                // B.cond / CBZ / CBNZ / TBZ / TBNZ have a 19/14-bit target too
                s64 off = 0;
                if ((insn & 0xFF000010) == 0x54000000) off = ((s32)((insn >> 5) << 13) >> 13);
                else if ((insn & 0x7E000000) == 0x34000000) off = ((s32)(((insn >> 5) & 0x7FFFF) << 13) >> 13);
                else off = ((s32)(((insn >> 5) & 0x3FFF) << 18) >> 18);
                u64 tt = pc + off * 4;
                if (tt >= base && (tt - base) / 4 < n) start[(u32)((tt - base) / 4)] = true;
            }
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

// register name: x31 reads as zero in most data-processing contexts.
std::string Xz(u32 r) { return r == 31 ? std::string("(uint64_t)0") : ("c->x[" + std::to_string(r) + "]"); }
std::string Wz(u32 r) { return r == 31 ? std::string("(uint32_t)0") : ("(uint32_t)c->x[" + std::to_string(r) + "]"); }
std::string Xset(u32 r) { return "c->x[" + std::to_string(r) + "]"; } // 31 == SP in SP-form

// ---- per-instruction translator --------------------------------------------
// Appends C statements for one instruction. Returns false if the instruction terminates the block.
bool Translate(u32 i, u64 pc, std::string& out) {
    char buf[256];
    auto emit = [&](const std::string& s) { out += "    " + s + "\n"; };
    const u64 next = pc + 4;

    if (i == 0xD503201F || (i & 0xFFFFF01F) == 0xD503201F) { emit("/* nop/hint */"); return true; } // NOP/HINT

    // MOVZ/MOVN/MOVK (move wide immediate)
    if ((i & 0x1F800000) == 0x12800000) {
        u32 sf = i >> 31, opc = (i >> 29) & 3, hw = (i >> 21) & 3, imm16 = (i >> 5) & 0xFFFF, rd = i & 31;
        if (rd != 31) {
            u64 shift = (u64)hw * 16;
            if (opc == 2) { // MOVZ
                snprintf(buf, sizeof buf, "c->x[%u] = (uint64_t)0x%llxULL;", rd, (unsigned long long)((u64)imm16 << shift));
                emit(buf);
            } else if (opc == 0) { // MOVN
                u64 v = ~((u64)imm16 << shift); if (!sf) v &= 0xFFFFFFFF;
                snprintf(buf, sizeof buf, "c->x[%u] = (uint64_t)0x%llxULL;", rd, (unsigned long long)v); emit(buf);
            } else if (opc == 3) { // MOVK
                snprintf(buf, sizeof buf, "c->x[%u] = (c->x[%u] & ~(0xFFFFULL<<%llu)) | (0x%xULL<<%llu);",
                         rd, rd, (unsigned long long)shift, imm16, (unsigned long long)shift); emit(buf);
            }
            if (!sf) { snprintf(buf, sizeof buf, "c->x[%u] &= 0xFFFFFFFFULL;", rd); emit(buf); }
        }
        return true;
    }

    // ADD/SUB immediate (incl S variants)
    if ((i & 0x1F000000) == 0x11000000) {
        u32 sf = i >> 31, op = (i >> 30) & 1, S = (i >> 29) & 1, sh = (i >> 22) & 1;
        u32 imm12 = (i >> 10) & 0xFFF, rn = (i >> 5) & 31, rd = i & 31;
        u64 imm = sh ? ((u64)imm12 << 12) : imm12;
        std::string a = (rn == 31) ? "c->x[31]" : ("c->x[" + std::to_string(rn) + "]"); // 31 = SP here
        snprintf(buf, sizeof buf, "{ uint64_t _a=%s, _b=%lluULL; uint64_t _r=%s; ", a.c_str(),
                 (unsigned long long)imm, op ? "_a-_b" : "_a+_b");
        std::string s = buf;
        if (!sf) s += "_r&=0xFFFFFFFFULL; ";
        if (rd != 31 || S) { if (rd != 31) s += "c->x[" + std::to_string(rd) + "]=_r; "; }
        else s += "c->x[31]=_r; ";
        if (S) s += "recomp_set_flags(c," + std::string(op ? "1" : "0") + ",_a,_b,_r," + (sf?"1":"0") + "); ";
        s += "}";
        emit(s);
        return true;
    }

    // Logical (shifted register): AND/ORR/EOR/ANDS, shift==LSL#0 fast path; MOV via ORR Xd,XZR,Xm
    if ((i & 0x1F000000) == 0x0A000000) {
        u32 sf = i >> 31, opc = (i >> 29) & 3, rm = (i >> 16) & 31, rn = (i >> 5) & 31, rd = i & 31;
        u32 shift = (i >> 22) & 3, imm6 = (i >> 10) & 0x3F, N = (i >> 21) & 1;
        std::string rmv = Xz(rm);
        if (imm6) { // apply shift
            const char* opn = shift==0?"<<":shift==1?">>":shift==2?">>":">>";
            char sb[96]; snprintf(sb,sizeof sb,"(%s %s %u)", rmv.c_str(), opn, imm6); rmv = sb;
        }
        std::string a = Xz(rn);
        const char* lop = opc==0?"&":opc==1?"|":opc==2?"^":"&";
        std::string expr = (N? ("("+a+" "+lop+" ~"+rmv+")") : ("("+a+" "+lop+" "+rmv+")"));
        if (rd != 31) {
            std::string s = "c->x[" + std::to_string(rd) + "] = " + expr + ";";
            emit(s);
            if (!sf) { snprintf(buf,sizeof buf,"c->x[%u]&=0xFFFFFFFFULL;",rd); emit(buf);}
        }
        if (opc==3) { snprintf(buf,sizeof buf,"recomp_set_logic_flags(c,%s,%s);", (rd!=31?("c->x["+std::to_string(rd)+"]").c_str():expr.c_str()), sf?"1":"0"); emit(buf);}
        return true;
    }

    // ADD/SUB shifted register (LSL/LSR/ASR), no extend
    if ((i & 0x1F200000) == 0x0B000000) {
        u32 sf=i>>31, op=(i>>30)&1, S=(i>>29)&1, shift=(i>>22)&3, rm=(i>>16)&31, imm6=(i>>10)&0x3F, rn=(i>>5)&31, rd=i&31;
        std::string rmv = Xz(rm);
        if (imm6) { const char* o=shift==0?"<<":">>"; char sb[96]; snprintf(sb,sizeof sb,"(%s %s %u)",rmv.c_str(),o,imm6); rmv=sb; }
        std::string a = Xz(rn);
        snprintf(buf,sizeof buf,"{ uint64_t _a=%s,_b=%s,_r=%s; ", a.c_str(), rmv.c_str(), op?"_a-_b":"_a+_b");
        std::string s=buf; if(!sf) s+="_r&=0xFFFFFFFFULL; ";
        if (rd!=31) s+="c->x["+std::to_string(rd)+"]=_r; ";
        if (S) s+="recomp_set_flags(c,"+std::string(op?"1":"0")+",_a,_b,_r,"+(sf?"1":"0")+"); ";
        s+="}"; emit(s); return true;
    }

    // ADRP / ADR
    if ((i & 0x1F000000) == 0x10000000) {
        u32 op=i>>31, rd=i&31; s64 immhi=(s32)(((i>>5)&0x7FFFF)<<13)>>13; u32 immlo=(i>>29)&3;
        if (rd!=31) {
            if (op) { u64 base=(pc & ~0xFFFULL); s64 imm=((immhi<<2)|immlo)<<12; snprintf(buf,sizeof buf,"c->x[%u]=0x%llxULL + (int64_t)%lld;",rd,(unsigned long long)base,(long long)imm); }
            else { s64 imm=(immhi<<2)|immlo; snprintf(buf,sizeof buf,"c->x[%u]=0x%llxULL + (int64_t)%lld;",rd,(unsigned long long)pc,(long long)imm); }
            emit(buf);
        }
        return true;
    }

    // LDR/STR (immediate, unsigned offset) 32/64; LDRB/STRB
    if ((i & 0x3B000000) == 0x39000000) {
        u32 size=(i>>30)&3, opc=(i>>22)&3, imm12=(i>>10)&0xFFF, rn=(i>>5)&31, rt=i&31;
        u64 off=(u64)imm12 << size;
        std::string addr = "c->x[" + std::to_string(rn) + "] + " + std::to_string(off);
        bool load = (opc & 1);
        const char* ty = size==0?"8":size==1?"16":size==2?"32":"64";
        if (load) {
            if (rt!=31){ snprintf(buf,sizeof buf,"c->x[%u]=recomp_load%s(c,%s);",rt,ty,addr.c_str()); emit(buf);}
        } else {
            snprintf(buf,sizeof buf,"recomp_store%s(c,%s,%s);",ty,addr.c_str(), Xz(rt).c_str()); emit(buf);
        }
        return true;
    }

    // ---- terminators ----
    u64 t=0;
    if (DirectBranchTarget(i,pc,t)) {
        if ((i & 0xFC000000) == 0x94000000) { snprintf(buf,sizeof buf,"c->x[30]=0x%llxULL;",(unsigned long long)next); emit(buf);} // BL sets LR
        snprintf(buf,sizeof buf,"c->pc=0x%llxULL; return;",(unsigned long long)t); emit(buf); return false;
    }
    if ((i & 0xFFFFFC1F) == 0xD65F0000) { emit("c->pc=c->x[30]; return; /* RET */"); return false; }
    if ((i & 0xFFFFFC1F) == 0xD61F0000) { u32 rn=(i>>5)&31; snprintf(buf,sizeof buf,"c->pc=c->x[%u]; return; /* BR */",rn); emit(buf); return false; }
    if ((i & 0xFFFFFC1F) == 0xD63F0000) { u32 rn=(i>>5)&31; snprintf(buf,sizeof buf,"c->x[30]=0x%llxULL; c->pc=c->x[%u]; return; /* BLR */",(unsigned long long)next,rn); emit(buf); return false; }
    if ((i & 0xFF000010) == 0x54000000) { // B.cond
        s64 off=((s32)((i>>5)<<13)>>13); u64 tt=pc+off*4; u32 cond=i&15;
        snprintf(buf,sizeof buf,"if (recomp_cond(c,%u)) { c->pc=0x%llxULL; } else { c->pc=0x%llxULL; } return;",cond,(unsigned long long)tt,(unsigned long long)next); emit(buf); return false;
    }
    if ((i & 0x7E000000) == 0x34000000) { // CBZ/CBNZ
        u32 sf=i>>31; bool nz=(i>>24)&1; u32 rt=i&31; s64 off=((s32)(((i>>5)&0x7FFFF)<<13)>>13); u64 tt=pc+off*4;
        std::string v= sf?Xz(rt):Wz(rt);
        snprintf(buf,sizeof buf,"if ((%s)%s0) { c->pc=0x%llxULL; } else { c->pc=0x%llxULL; } return;",v.c_str(), nz?"!=":"==",(unsigned long long)tt,(unsigned long long)next); emit(buf); return false;
    }
    if ((i & 0x7E000000) == 0x36000000) { // TBZ/TBNZ
        bool nz=(i>>24)&1; u32 b=((i>>31)<<5)|((i>>19)&31); u32 rt=i&31; s64 off=((s32)(((i>>5)&0x3FFF)<<18)>>18); u64 tt=pc+off*4;
        snprintf(buf,sizeof buf,"if (((c->x[%u]>>%u)&1)%s0) { c->pc=0x%llxULL; } else { c->pc=0x%llxULL; } return;",rt,b, nz?"!=":"==",(unsigned long long)tt,(unsigned long long)next); emit(buf); return false;
    }
    if ((i & 0xFFE0001F) == 0xD4000001) { // SVC
        u32 imm=(i>>5)&0xFFFF; snprintf(buf,sizeof buf,"c->pc=0x%llxULL; recomp_svc(c,%u); return;",(unsigned long long)next,imm); emit(buf); return false;
    }

    // fallback
    snprintf(buf,sizeof buf,"recomp_unhandled(c,0x%08xU,0x%llxULL); c->pc=0x%llxULL; return;",i,(unsigned long long)pc,(unsigned long long)next);
    emit(buf); return false;
}

std::string FuncName(u64 v){ char b[32]; snprintf(b,sizeof b,"blk_%016llx",(unsigned long long)v); return b; }

} // namespace

// runtime templates ----------------------------------------------------------
static const char* RUNTIME_H = R"RT(
#ifndef SUYU_RECOMP_RUNTIME_H
#define SUYU_RECOMP_RUNTIME_H
#include <stdint.h>
typedef struct GuestContext {
    uint64_t x[32];   /* x[31] = SP */
    uint64_t pc;
    uint8_t n,z,c,v;  /* NZCV */
    uint8_t* mem;     /* flat guest memory base */
    uint64_t mem_size;
    uint64_t mem_base_vaddr;
    int halted;
} GuestContext;
typedef void (*BlockFn)(GuestContext*);
/* generated dispatch */
BlockFn recomp_lookup(uint64_t pc);
void    recomp_run(GuestContext* c);
/* helpers used by generated code */
void recomp_set_flags(GuestContext* c, int is_sub, uint64_t a, uint64_t b, uint64_t r, int is64);
void recomp_set_logic_flags(GuestContext* c, uint64_t r, int is64);
int  recomp_cond(GuestContext* c, unsigned cond);
uint64_t recomp_load8(GuestContext*,uint64_t); uint64_t recomp_load16(GuestContext*,uint64_t);
uint64_t recomp_load32(GuestContext*,uint64_t); uint64_t recomp_load64(GuestContext*,uint64_t);
void recomp_store8(GuestContext*,uint64_t,uint64_t); void recomp_store16(GuestContext*,uint64_t,uint64_t);
void recomp_store32(GuestContext*,uint64_t,uint64_t); void recomp_store64(GuestContext*,uint64_t,uint64_t);
void recomp_svc(GuestContext* c, unsigned imm);
void recomp_unhandled(GuestContext* c, uint32_t insn, uint64_t pc);
#endif
)RT";

static const char* RUNTIME_C = R"RT(
#include "recomp_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* HLE hook point: a "modified suyu runtime" would route SVCs to suyu's kernel HLE here.
   Standalone, we print and halt so recompiled native code is observable. */
static uint8_t* memptr(GuestContext* c, uint64_t va, uint64_t sz){
    uint64_t off = va - c->mem_base_vaddr;
    if (off + sz > c->mem_size) return 0;
    return c->mem + off;
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
    uint64_t mask = is64?~0ULL:0xFFFFFFFFULL; r&=mask; a&=mask; b&=mask;
    uint64_t sign = is64?0x8000000000000000ULL:0x80000000ULL;
    c->z = (r==0); c->n = (r&sign)?1:0;
    if(is_sub){ c->c = (a>=b); c->v = (((a^b)&(a^r))&sign)?1:0; }
    else { c->c = (r<a); c->v = ((~(a^b)&(a^r))&sign)?1:0; }
}
void recomp_set_logic_flags(GuestContext* c,uint64_t r,int is64){ uint64_t m=is64?~0ULL:0xFFFFFFFFULL; r&=m; uint64_t s=is64?0x8000000000000000ULL:0x80000000ULL; c->z=(r==0); c->n=(r&s)?1:0; c->c=0; c->v=0; }
int recomp_cond(GuestContext* c,unsigned cond){
    int n=c->n,z=c->z,cc=c->c,v=c->v,res;
    switch(cond>>1){case 0:res=z;break;case 1:res=cc;break;case 2:res=n;break;case 3:res=v;break;
      case 4:res=cc&&!z;break;case 5:res=(n==v);break;case 6:res=(n==v)&&!z;break;default:res=1;}
    return (cond&1)&&cond!=15? !res : res;
}
void recomp_svc(GuestContext* c, unsigned imm){
    /* In an integrated build this dispatches to suyu's HLE SVC handler.
       Standalone demo: svc #0 prints x0..x3 and halts. */
    printf("[recomp] SVC #%u  x0=%llu x1=%llu x2=%llu x3=%llu\n", imm,
        (unsigned long long)c->x[0],(unsigned long long)c->x[1],
        (unsigned long long)c->x[2],(unsigned long long)c->x[3]);
    c->halted = 1;
}
void recomp_unhandled(GuestContext* c, uint32_t insn, uint64_t pc){
    fprintf(stderr,"[recomp] unhandled insn 0x%08x at 0x%llx -> halting (needs full suyu HLE/JIT)\n",
        insn,(unsigned long long)pc);
    c->halted = 1;
}
void recomp_run(GuestContext* c){
    int guard=0;
    while(!c->halted){
        BlockFn f = recomp_lookup(c->pc);
        if(!f){ fprintf(stderr,"[recomp] no block at 0x%llx\n",(unsigned long long)c->pc); break; }
        f(c);
        if(++guard > 100000000){ fprintf(stderr,"[recomp] watchdog\n"); break; }
    }
}
)RT";

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr,"usage: %s <input.bin> <base_hex> <out_dir> [--source-only]\n", argv[0]);
        fprintf(stderr,"  input.bin = raw AArch64 .text bytes; base_hex = load vaddr (e.g. 0x1000)\n");
        return 2;
    }
    const std::string in = argv[1];
    const u64 base = strtoull(argv[2], nullptr, 0);
    const std::string out = argv[3];
    const bool source_only = argc > 4 && std::string(argv[4]) == "--source-only";

    std::ifstream f(in, std::ios::binary);
    if (!f) { fprintf(stderr,"cannot open %s\n", in.c_str()); return 1; }
    std::vector<u8> text((std::istreambuf_iterator<char>(f)), {});
    if (text.size() < 4) { fprintf(stderr,"input too small\n"); return 1; }
    text.resize(text.size() & ~size_t(3));

    auto blocks = DiscoverBlocks(text.data(), text.size(), base);
    const u32* p = reinterpret_cast<const u32*>(text.data());

    // emit recompiled.c
    std::ostringstream rc;
    rc << "/* auto-generated by suyu_recomp - DO NOT EDIT */\n#include \"recomp_runtime.h\"\n\n";
    for (const auto& b : blocks) {
        rc << "void " << FuncName(b.vaddr) << "(GuestContext* c){\n";
        const u32 first = (u32)((b.vaddr - base) / 4);
        bool open = true;
        for (u32 k = 0; k < b.count; ++k) {
            const u64 pc = b.vaddr + (u64)k * 4;
            std::string body;
            open = Translate(p[first + k], pc, body);
            rc << body;
            if (!open) break;
        }
        if (open) { rc << "    c->pc=0x" << std::hex << (b.vaddr + b.size) << std::dec << "ULL; return;\n"; }
        rc << "}\n\n";
    }
    // dispatch table
    rc << "#include <stdint.h>\nstruct _ent{uint64_t va; BlockFn fn;};\n";
    rc << "static const struct _ent _tbl[] = {\n";
    for (const auto& b : blocks)
        rc << "  {0x" << std::hex << b.vaddr << std::dec << "ULL, " << FuncName(b.vaddr) << "},\n";
    rc << "};\nBlockFn recomp_lookup(uint64_t pc){ for(unsigned i=0;i<sizeof(_tbl)/sizeof(_tbl[0]);++i) if(_tbl[i].va==pc) return _tbl[i].fn; return 0; }\n";

    // emit main.c (loads the same .text as guest memory so PC-relative loads work)
    std::ostringstream mc;
    mc << "#include \"recomp_runtime.h\"\n#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n";
    mc << "int main(int argc,char**argv){\n  static uint8_t mem[1<<20];\n  GuestContext c; memset(&c,0,sizeof c);\n";
    mc << "  c.mem=mem; c.mem_size=sizeof mem; c.mem_base_vaddr=0x" << std::hex << base << std::dec << "ULL;\n";
    mc << "  c.x[31]=c.mem_base_vaddr + c.mem_size - 16; /* SP */\n";
    mc << "  c.pc=0x" << std::hex << base << std::dec << "ULL;\n";
    mc << "  recomp_run(&c);\n  printf(\"[recomp] halted at pc=0x%llx\\n\",(unsigned long long)c.pc);\n  return 0;\n}\n";

    // emit CMakeLists (cross-platform: Windows exe / Linux+BSD ELF / macOS Mach-O)
    std::ostringstream cm;
    cm << "cmake_minimum_required(VERSION 3.13)\nproject(suyu_recompiled C)\n"
       << "set(CMAKE_C_STANDARD 11)\nadd_executable(recompiled main.c recompiled.c recomp_runtime.c)\n"
       << "if(WIN32)\n  set_target_properties(recompiled PROPERTIES OUTPUT_NAME recompiled SUFFIX \".exe\")\n"
       << "elseif(APPLE)\n  # produces a Mach-O native binary\nelse()\n  # Linux / FreeBSD / OpenBSD: ELF\nendif()\n";

    auto write = [&](const std::string& name, const std::string& data){
        std::ofstream o(out + "/" + name, std::ios::binary);
        o.write(data.data(), (std::streamsize)data.size());
    };
    // create out dir (portable best-effort)
    { std::string mk =
#ifdef _WIN32
        "cmd /c if not exist \"" + out + "\" mkdir \"" + out + "\"";
#else
        "mkdir -p '" + out + "'";
#endif
      system(mk.c_str()); }

    write("recomp_runtime.h", RUNTIME_H);
    write("recomp_runtime.c", RUNTIME_C);
    write("recompiled.c", rc.str());
    write("main.c", mc.str());
    write("CMakeLists.txt", cm.str());

    fprintf(stderr,"suyu_recomp: %zu blocks, %zu instructions -> %s (%s)\n",
        blocks.size(), text.size()/4, out.c_str(), source_only?"source only":"buildable project");
    return 0;
}
