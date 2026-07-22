
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
