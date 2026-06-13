#include "recomp_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char**argv){
  static uint8_t mem[1<<20];
  GuestContext c; memset(&c,0,sizeof c);
  c.mem=mem; c.mem_size=sizeof mem; c.mem_base_vaddr=0x1000ULL;
  c.x[31]=c.mem_base_vaddr + c.mem_size - 16; /* SP */
  c.pc=0x1000ULL;
  recomp_run(&c);
  printf("[recomp] halted at pc=0x%llx\n",(unsigned long long)c.pc);
  return 0;
}
