//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *p = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd] == 0){
      p->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

uint64
sys_dup(void)
{
  struct file *f;
  int fd;

  if(argfd(0, 0, &f) < 0)
    return -1;
  if((fd=fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

uint64
sys_read(void)
{
  struct file *f;
  int n;
  uint64 p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
    return -1;
  return fileread(f, p, n);
}

uint64
sys_write(void)
{
  struct file *f;
  int n;
  uint64 p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
    return -1;

  return filewrite(f, p, n);
}

uint64
sys_close(void)
{
  int fd;
  struct file *f;

  if(argfd(0, &fd, &f) < 0)
    return -1;
  myproc()->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

uint64
sys_fstat(void)
{
  struct file *f;
  uint64 st; // user pointer to struct stat

  if(argfd(0, 0, &f) < 0 || argaddr(1, &st) < 0)
    return -1;
  return filestat(f, st);
}

// Create the path new as a link to the same inode as old.
uint64
sys_link(void)
{
  char name[DIRSIZ], new[MAXPATH], old[MAXPATH];
  struct inode *dp, *ip;

  if(argstr(0, old, MAXPATH) < 0 || argstr(1, new, MAXPATH) < 0)
    return -1;

  begin_op(ROOTDEV);
  if((ip = namei(old)) == 0){
    end_op(ROOTDEV);
    return -1;
  }

  ilock(ip);
  if(ip->type == T_DIR){
    iunlockput(ip);
    end_op(ROOTDEV);
    return -1;
  }

  ip->nlink++;
  iupdate(ip);
  iunlock(ip);

  if((dp = nameiparent(new, name)) == 0)
    goto bad;
  ilock(dp);
  if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
    iunlockput(dp);
    goto bad;
  }
  iunlockput(dp);
  iput(ip);

  end_op(ROOTDEV);

  return 0;

bad:
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  end_op(ROOTDEV);
  return -1;
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
  int off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if(de.inum != 0)
      return 0;
  }
  return 1;
}

uint64
sys_unlink(void)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], path[MAXPATH];
  uint off;

  if(argstr(0, path, MAXPATH) < 0)
    return -1;

  begin_op(ROOTDEV);
  if((dp = nameiparent(path, name)) == 0){
    end_op(ROOTDEV);
    return -1;
  }

  ilock(dp);

  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(ip->type == T_DIR && !isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  if(writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(ip->type == T_DIR){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  end_op(ROOTDEV);

  return 0;

bad:
  iunlockput(dp);
  end_op(ROOTDEV);
  return -1;
}

static struct inode*
create(char *path, short type, short major, short minor)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;

  ilock(dp);

  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    if(type == T_FILE && (ip->type == T_FILE || ip->type == T_DEVICE))
      return ip;
    iunlockput(ip);
    return 0;
  }

  if((ip = ialloc(dp->dev, type)) == 0)
    panic("create: ialloc");

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    dp->nlink++;  // for ".."
    iupdate(dp);
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      panic("create dots");
  }

  if(dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");

  iunlockput(dp);

  return ip;
}

uint64
sys_open(void)
{
  char path[MAXPATH];
  int fd, omode;
  struct file *f;
  struct inode *ip;
  int n;

  if((n = argstr(0, path, MAXPATH)) < 0 || argint(1, &omode) < 0)
    return -1;

  begin_op(ROOTDEV);

  if(omode & O_CREATE){
    ip = create(path, T_FILE, 0, 0);
    if(ip == 0){
      end_op(ROOTDEV);
      return -1;
    }
  } else {
    if((ip = namei(path)) == 0){
      end_op(ROOTDEV);
      return -1;
    }
    ilock(ip);
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op(ROOTDEV);
      return -1;
    }
  }

  if(ip->type == T_DEVICE && (ip->major < 0 || ip->major >= NDEV)){
    iunlockput(ip);
    end_op(ROOTDEV);
    return -1;
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    iunlockput(ip);
    end_op(ROOTDEV);
    return -1;
  }

  if(ip->type == T_DEVICE){
    f->type = FD_DEVICE;
    f->major = ip->major;
    f->minor = ip->minor;
  } else {
    f->type = FD_INODE;
  }
  f->ip = ip;
  f->off = 0;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

  iunlock(ip);
  end_op(ROOTDEV);

  return fd;
}

uint64
sys_mkdir(void)
{
  char path[MAXPATH];
  struct inode *ip;

  begin_op(ROOTDEV);
  if(argstr(0, path, MAXPATH) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0){
    end_op(ROOTDEV);
    return -1;
  }
  iunlockput(ip);
  end_op(ROOTDEV);
  return 0;
}

uint64
sys_mknod(void)
{
  struct inode *ip;
  char path[MAXPATH];
  int major, minor;

  begin_op(ROOTDEV);
  if((argstr(0, path, MAXPATH)) < 0 ||
     argint(1, &major) < 0 ||
     argint(2, &minor) < 0 ||
     (ip = create(path, T_DEVICE, major, minor)) == 0){
    end_op(ROOTDEV);
    return -1;
  }
  iunlockput(ip);
  end_op(ROOTDEV);
  return 0;
}

uint64
sys_chdir(void)
{
  char path[MAXPATH];
  struct inode *ip;
  struct proc *p = myproc();
  
  begin_op(ROOTDEV);
  if(argstr(0, path, MAXPATH) < 0 || (ip = namei(path)) == 0){
    end_op(ROOTDEV);
    return -1;
  }
  ilock(ip);
  if(ip->type != T_DIR){
    iunlockput(ip);
    end_op(ROOTDEV);
    return -1;
  }
  iunlock(ip);
  iput(p->cwd);
  end_op(ROOTDEV);
  p->cwd = ip;
  return 0;
}

uint64
sys_exec(void)
{
  char path[MAXPATH], *argv[MAXARG];
  int i;
  uint64 uargv, uarg;

  if(argstr(0, path, MAXPATH) < 0 || argaddr(1, &uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv)){
      goto bad;
    }
    if(fetchaddr(uargv+sizeof(uint64)*i, (uint64*)&uarg) < 0){
      goto bad;
    }
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    argv[i] = kalloc();
    if(argv[i] == 0)
      panic("sys_exec kalloc");
    if(fetchstr(uarg, argv[i], PGSIZE) < 0){
      goto bad;
    }
  }

  int ret = exec(path, argv);

  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);

  return ret;

 bad:
  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);
  return -1;
}

uint64
sys_pipe(void)
{
  uint64 fdarray; // user pointer to array of two integers
  struct file *rf, *wf;
  int fd0, fd1;
  struct proc *p = myproc();

  if(argaddr(0, &fdarray) < 0)
    return -1;
  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      p->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  if(copyout(p->pagetable, fdarray, (char*)&fd0, sizeof(fd0)) < 0 ||
     copyout(p->pagetable, fdarray+sizeof(fd0), (char *)&fd1, sizeof(fd1)) < 0){
    p->ofile[fd0] = 0;
    p->ofile[fd1] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  return 0;
}



/** 
 On success, mmap() returns a pointer to the mapped area.  
 On error, the value MAP_FAILED (that is, (void *) -1) is re‐
 turned, and errno is set to indicate the cause of the error.

 On success, munmap() returns 0.  
 On failure, it returns -1, and errno is set to indicate the cause of the error (prob‐
 ably to EINVAL).
 
 */
/**
 * 
 * void *mmap(void *addr, size_t length, int prot, int flags,
 *                 int fd, off_t offset);
 */
/**
 * 思路参考自：
 * https://xiayingp.gitbook.io/build_a_os/labs/untitled#define-vma-virtual-memory-area-per-process
 * 
 * 如何 find an unused region in the process's address space in which to map the file,？
 * xv6 book Figure 3.4 给出： A process’s user address space, with its initial stack.
 * 
 * 因此，在mmap中，我们试图从上往下分配地址。
 * 
 * 为了实现：
 * 1. 在proc中添加一个vma管理器
 * 2. 在proc中添加一个当前最大虚拟地址
 * 3. 在proc中记录当前最大虚拟地址属于哪一个vma
 */
uint64
sys_mmap(void){
  uint64 addr;
  int length;
  int prot;
  int flags;
  int fd;
  struct file* f;
  int offset;
  
  if(argaddr(0, &addr) < 0 || argint(1, &length) < 0 || 
     argint(2, &prot) < 0 || argint(3, &flags) < 0 || 
     argfd(4, &fd, &f) < 0 || argint(5, &offset) < 0){
    return -1;
  }

  if(!f->writable && (prot & PROT_WRITE) && (flags & MAP_SHARED)){
    return -1;
  }
  /**
   * 
   * you can assume that addr will always be zero, 
   * meaning that the kernel should decide the virtual address at which to map the file
   * 
   * offset it's the starting point in the file at which to map
   */
  struct proc* p;
  p = myproc();
  
  struct VMA* vma = 0;
  /** 从上往下找到第一个可用的vma  */
  for (int i = NVMA - 1; i >= 0; i--)
  {
    if(p->vmas[i].vm_valid){
      vma = &p->vmas[i];
      /** 置当前的imaxvma为i  */
      p->current_imaxvma = i;
      break;
    }
  }
  /**
   * 1. VMA：START在下方
   * 原因：kalloc是分配内存向上增长，因此start要在下方
   * ->current_maxva     0x001xx END
   *                     ............
   *                     0x000xx START
   * 
   * ----------------------------------
   * 2. 更新current_maxva
   *                     0x001xx END
   *                     ............
   * ->current_maxva     0x000xx START
   * ----------------------------------
   */
  if(vma){
    /** 记得这里要用uint64，否则会做最高位拓展  */
    printf("sys_mmap(): %p, length: %d\n",p->current_maxva, length);
    uint64 vm_end = PGROUNDDOWN(p->current_maxva);
    uint64 vm_start = PGROUNDDOWN(p->current_maxva - length);
    printf("vm_start(): %p, vm_end: %p\n",vm_start, vm_end);
    vma->vm_valid = 0;
    vma->vm_fd = fd;
    vma->vm_file = f;
    vma->vm_flags = flags;
    vma->vm_prot = prot;
    vma->vm_end = vm_end;
    vma->vm_start = vm_start;
    /**
     * mmap should increase the file's reference count 
     * so that the structure doesn't disappear when the file is closed (hint: see filedup).
     */
    vma->vm_file->ref++;
    p->current_maxva = vm_start;
  }
  else
  {
    return -1;
  }  
  return vma->vm_start;
}

/**
 * int munmap(void *addr, size_t length);
 * 
 * 
 * An munmap call might cover only a portion of an mmap-ed region, but you can assume that 
 * it will either unmap at the start, 
 * or at the end, or the whole region (but not punch a hole in the middle of a region).
 */
uint64
sys_munmap(void){
  uint64 addr;
  int length;
  if(argaddr(0, &addr) < 0 || argint(1, &length) < 0){
    return -1;
  }
  printf("### sys_munmap: \n");
  printf("addr: %p, length:%d, current:%p\n", addr, length, myproc()->current_maxva);
  struct proc* p = myproc();
  for (int i = NVMA - 1; i >= 0; i--)
  {
    if(p->vmas[i].vm_start <= addr && addr <= p->vmas[i].vm_end){
      struct VMA* vma = &p->vmas[i];
      /** 
       * 1. If an unmapped page has been modified and the file is mapped MAP_SHARED, 
       * write the page back to the file. Look at filewrite for inspiration.  
       * 
       * 
       * 2. However, mmaptest does not check that non-dirty pages are not written back; 
       * thus you can get away with writing pages back without looking at D bits.
       * 
       * 
       * 1. unmap的时候，只会从一个vma的起始开始，因此，可以默认p->vmas[i].vm_start = addr，因此
       *    我们后面有一个vma->vm_start += length操作。
       * 2. 就是说，碰到MAP_SHARED就回写，不用理会“写脏D位”
       * 
       * 3. 指针current_maxva基于current_imaxvma紧缩，这样是一个折中的办法，而不是一直向下增长
       * 
       * */
      /** 首先要判断  */
      if(walkaddr(p->pagetable, vma->vm_start)){
        if(vma->vm_flags == MAP_SHARED){
          printf("sys_munmap(): write back \n");
          /** 回写文件  */
          filewrite(vma->vm_file, vma->vm_start, length);
        }
        uvmunmap(p->pagetable, vma->vm_start, length ,1);
      }

      vma->vm_start += length;
      printf("vma_start: %p, vma_end: %p\n", vma->vm_start, vma->vm_end);
      if(vma->vm_start == vma->vm_end){
        vma->vm_file->ref--;
        /** 置该块可用  */
        vma->vm_valid = 1;
      }

      /** Shrink  */
      int j;
      /** 紧缩 p->current_maxva */
      for (j = p->current_imaxvma; j < NVMA; j++)
      {
        if(!p->vmas[j].vm_valid){
          p->current_maxva = p->vmas[j].vm_start;
          p->current_imaxvma = j;
          break;
        }
      }
      if(j == NVMA){
        p->current_maxva = VMASTART;
      }
      return 0;
    }
  }
  
  printf("################ arrive at munmap!\n");
  return -1;
}


