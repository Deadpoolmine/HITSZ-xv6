
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
pgbug(char *s)
{
  char *argv[1];
  argv[0] = 0;
  exec((char*)0xeaeb0b5b00002f5e, argv);

  pipe((int*)0xeaeb0b5b00002f5e);

  exit(0);
}

int 
main(int argc, char const *argv[])
{
    /* code */
    pgbug("pgbug");
    return 0;
}
