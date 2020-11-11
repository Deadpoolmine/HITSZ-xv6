#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/syscall.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"

void
sbrkmuch(char *s)
{
  enum { BIG=100*1024*1024 };
  char *c, *oldbrk, *a, *lastaddr, *p;
  uint64 amt;

  oldbrk = sbrk(0);

  // can one grow address space to something big?
  a = sbrk(0);
  amt = BIG - (uint64)a;
  p = sbrk(amt);
  if (p != a) {
    printf("%s: sbrk test failed to grow big address space; enough phys mem?\n", s);
    exit(1);
  }
  lastaddr = (char*) (BIG-1);
  *lastaddr = 99;

  // can one de-allocate?
  a = sbrk(0);
  c = sbrk(-PGSIZE);
  if(c == (char*)0xffffffffffffffffL){
    printf("%s: sbrk could not deallocate\n", s);
    exit(1);
  }
  c = sbrk(0);
  if(c != a - PGSIZE){
    printf("%s: sbrk deallocation produced wrong address, a %x c %x\n", a, c);
    exit(1);
  }

  // can one re-allocate that page?
  a = sbrk(0);
  c = sbrk(PGSIZE);
  if(c != a || sbrk(0) != a + PGSIZE){
    printf("%s: sbrk re-allocation failed, a %x c %x\n", a, c);
    exit(1);
  }
  if(*lastaddr == 99){
    // should be zero
    printf("%s: sbrk de-allocation didn't really deallocate\n", s);
    exit(1);
  }

  a = sbrk(0);
  c = sbrk(-(sbrk(0) - oldbrk));
  if(c != a){
    printf("%s: sbrk downsize failed, a %x c %x\n", a, c);
    exit(1);
  }
}

int 
main() {
    sbrkmuch("sbrkmuch");
    return 0;
}