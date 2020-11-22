// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#define NPAGE 32723

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.
/** 
 * Next, 
 * ensure that each physical page is freed when the last 
 * PTE reference to it goes away (but not before!), 
 * perhaps by implementing reference counts in kalloc.c.   */


char reference[NPAGE];

struct run {
  // int ref_count;  /** 添加referece */
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;


int
getrefindex(void *pa){
  int index = ((char*)pa - (char*)PGROUNDUP((uint64)end)) / PGSIZE;
  return index;
}

int
getref(void *pa){
  return reference[getrefindex(pa)];
}


void
addref(char *tip, void *pa){
  
  reference[getrefindex(pa)]++;
  // printf("%s: addref: %d, pa: %p \n",tip,  reference[index], pa); 
  //((struct run*)pa)->ref_count++;
  //printf("%s: addref: %d, pa: %p \n", tip, ((struct run*)pa)->ref_count, pa);
}

void
subref(char *tip,void *pa){
  int index = getrefindex(pa);
  if(reference[index] == 0)
    return;
  reference[index]--;
  //kfree(pa);
  //printf("%s: subref: %d, pa: %p \n",tip,  reference[index], pa); 
  /* if(((struct run*)pa)->ref_count == 0){
    return;
  }
  ((struct run*)pa)->ref_count--;
  printf("%s: subref: %d, pa: %p \n",tip, ((struct run*)pa)->ref_count,pa); */
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  printf("start ~ end:%p ~ %p\n", p, pa_end);
  //int index = 0;
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    
    /** 初始化ref_count  */
    reference[getrefindex(p)] = 0;
    //((struct run *)p)->ref_count = 0;
    //printf("r->ref_count: %d\n",((struct run *)p)->ref_count);
    kfree(p);
    // printf("r->ref_count: %d\n",((struct run *)p)->ref_count);
  }
  //printf("index: %d\n", index);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  
  // printf("----------------\n");
  // printf("r->ref_count befroe: %d\n",((struct run *)pa)->ref_count);
  // Fill with junk to catch dangling refs.
  /** implementation of ref count  */
  //int ref_count = ((struct run *)pa)->ref_count;
  /** 
   * 
   * 大坑：一定要在kfree中减，因为其他很多程序也会调用kfree，如此一来，那些程序就无法kfree掉
   * 鸣谢：刘俊杰同学
   * */
  subref("kfree()", (void *) pa);
  int ref_count = getref(pa);
  if(ref_count == 0){
    //printf("!\n");
    memset(pa, 1, PGSIZE);
    // printf("r->ref_count after: %d\n",((struct run *)pa)->ref_count);
    // printf("----------------\n");
    r = (struct run*)pa;
    //r->ref_count = ref_count;
    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);

  }
  
  //printf("awidjawldjwalidaji:%d\n",);

  /* memset(pa, 1, PGSIZE);
  // printf("r->ref_count after: %d\n",((struct run *)pa)->ref_count);
  // printf("----------------\n");
  
  r = (struct run*)pa;
  // r->ref_count = 0;
  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock); */
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);
  /** implementation of ref count  */
  /** r is the start of physical page  */
  if(r){
    //int ref_count = r->ref_count;
    //printf("r->ref_count: %d\n",ref_count);
    memset((char*)r, 5, PGSIZE); // fill with junk
    int index = getrefindex((void *)r);
    reference[index] = 1;
    /** 由于kalloc只会被同一块内存调用1次，因此r的ref置为  */
    //r->ref_count = ref_count + 1; 
    //printf("r->ref_count: %d\n",ref_count);  
  }
  /** r出去后会被修改 */
  return (void*)r;
}


