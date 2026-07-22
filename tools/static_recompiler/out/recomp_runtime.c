
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
