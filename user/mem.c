#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/syscall.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"

// allocate all mem, free it, and allocate again
void
mem(char *s)
{
  void *m1, *m2;
  int pid;

  if((pid = fork()) == 0){
    m1 = 0;
    while((m2 = malloc(10001)) != 0){
      *(char**)m2 = m1;
      m1 = m2;
    }
    while(m1){
      m2 = *(char**)m1;
      free(m1);
      m1 = m2;
    }
    m1 = malloc(1024*20);
    if(m1 == 0){
      printf("couldn't allocate mem?!!\n", s);
      exit(1);
    }
    free(m1);
    exit(0);
  } else {
    int xstatus;
    wait(&xstatus);
    exit(xstatus);
  }
}

int main(int argc, char const *argv[])
{
    mem("mem");
    return 0;
}
