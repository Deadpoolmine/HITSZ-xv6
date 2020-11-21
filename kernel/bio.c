// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUKETS 13

struct {
  struct spinlock lock;
  struct buf buf[NBUF];             // block[30]
  /** 实则循环双向链表  */
  // Linked list of all buffers, through prev/next. 
  // head.next is most recently used.
  //struct buf head;

  struct buf buckets[NBUKETS];
  struct spinlock bucketslock[NBUKETS];

} bcache;

int getHb(struct buf *b){
  return b->blockno % NBUKETS;
}

int getH(uint blockno){
  return blockno % NBUKETS;
}

void checkbuckets(){
  struct buf *b;
  for (int i = 0; i < NBUKETS; i++)
  {
    printf("# bucket %d:", i);
    for(b = bcache.buckets[i].next; b != &bcache.buckets[i]; b = b->next){
      printf("%d ",b->blockno);
    }
    printf("\n");
  }
  
}

/**
 * We suggest you look up block numbers in the cache 
 * with a hash table that has a lock per hash bucket. 
 * */

/**
 * Hints
 * 1. Read the description of the block cache in the xv6 book (Section 7.2).
   
   2. It is OK to use a fixed number of buckets and not resize the hash table dynamically. 
      Use a prime number of buckets (e.g., 13) to reduce the likelihood of hashing conflicts.
   
   3. Searching in the hash table for a buffer and allocating an entry for that buffer 
      when the buffer is not found must be atomic.
   
   4. Remove the list of all buffers (bcache.head etc.) and instead time-stamp buffers 
      using the time of their last use (i.e., using ticks in kernel/trap.c). 
      With this change brelse doesn't need to acquire the bcache lock, 
      and bget can select the least-recently used block based on the time-stamps.
   
   5. It is OK to serialize eviction(回收/收回) in bget 
      (i.e., the part of bget that selects a buffer to re-use when a lookup misses in the cache).
   
   6. Your solution might need to hold two locks in some cases; for example, 
      during eviction you may need to hold the bcache lock and a lock per bucket. Make sure you avoid deadlock.
   
   7. When replacing a block, you might move a struct buf from one bucket to another bucket, 
      because the new block hashes to a different bucket. You might have a tricky case: 
      the new block might hash to the same bucket as the old block. Make sure you avoid deadlock in that case.
 */

//Modify **bget** and **brelse** so that concurrent lookups and releases 
//for different blocks that 
//are in the bcache are unlikely to conflict on locks

void
binit(void)
{
  struct buf *b;
  
  

  // Create linked list of buffers
  /**
   * All other access to the buffer cache refer to 
   * the linked list via bcache.head, not the buf array.
   */
  /** 在head头插入b  */
  initlock(&bcache.lock, "bcache");
  /* 
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf + NBUF; b++){
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  } */
  
  for (int i = 0; i < NBUKETS; i++)
  {
    initlock(&bcache.bucketslock[i], "bcache.bucket");
    bcache.buckets[i].prev = &bcache.buckets[i];
    bcache.buckets[i].next = &bcache.buckets[i];
  }

  for (b = bcache.buf; b < bcache.buf + NBUF; b++)
  {
    int hash = getHb(b);
    b->time_stamp = ticks;
    b->next = bcache.buckets[hash].next;
    b->prev = &bcache.buckets[hash];
    initsleeplock(&b->lock, "buffer");
    bcache.buckets[hash].next->prev = b;
    bcache.buckets[hash].next = b;
  }

  //printf("# end of binit \n");
}


// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.

// Bget (kernel/bio.c:70) scans the buffer list for a buffer with the given device and sector numbers
// (kernel/bio.c:69-84). If there is such a buffer, bget acquires the sleep-lock for the buffer. Bget then
// returns the locked buffer
// sector：扇区
static struct buf*
bget(uint dev, uint blockno)
{
  int hash = getH(blockno);
  struct buf *b;
  //printf("# request dev = %d & blockno = %d \n", dev, blockno);
  //acquire(&bcache.lock);
  
  // Is the block already cached?
 /*  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  } */
  //checkbuckets();
  /**
   * 
   * My modification
   */
  acquire(&bcache.bucketslock[hash]);

  for(b = bcache.buckets[hash].next; b != &bcache.buckets[hash]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->time_stamp = ticks;
      b->refcnt++;
      //printf("## end has \n");
      release(&bcache.bucketslock[hash]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  
  // If there is no cached buffer for the given sector, bget must make one, possibly reusing a buffer
  // that held a different sector. It scans the buffer list a second time, looking for a buffer that 
  // is not in use
  // (b->refcnt = 0); any such buffer can be used. Bget edits the buffer metadata to record the new
  // device and sector number and acquires its sleep-lock. Note that the assignment b->valid = 0
  // ensures that bread will read the block data from disk rather than incorrectly using the buffer’s
  // previous contents.

  // Not cached; recycle an unused buffer.
  
  /* for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;     //important  
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  } */
  /**
   * 
   * My modification
   */
  for (int i = 0; i < NBUKETS; i++)
  {
    if(i != hash){
      acquire(&bcache.bucketslock[i]);
      for(b = bcache.buckets[i].prev; b != &bcache.buckets[i]; b = b->prev){
        if(b->refcnt == 0){
          b->time_stamp = ticks;
          b->dev = dev;
          b->blockno = blockno;
          b->valid = 0;     //important  
          b->refcnt = 1;
          
          /** 将b脱出  */
          b->next->prev = b->prev;
          b->prev->next = b->next;
          
          /** 将b接入  */
          b->next = bcache.buckets[hash].next;
          b->prev = &bcache.buckets[hash];
          bcache.buckets[hash].next->prev = b;
          bcache.buckets[hash].next = b;
          //printf("## end alloc: hash: %d, has: %d\n", hash,i);
          release(&bcache.bucketslock[i]);
          release(&bcache.bucketslock[hash]);
          acquiresleep(&b->lock);
          return b;
        }
      }
      release(&bcache.bucketslock[i]);
    }
  }
  
  /* for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;     //important  
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  } */

  
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
// Bread (kernel/bio.c:91) calls bget to get a buffer for the given sector (kernel/bio.c:95). If the
// buffer needs to be read from disk, bread calls virtio_disk_rw to do that before returning the
// buffer.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b->dev, b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b->dev, b, 1);
}

// Release a locked buffer.
// Move to the head of the MRU list.
void
brelse(struct buf *b)
{
  //printf("#---------------------------------------- brelse! ----------------------------------------\n");
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  /* acquire(&bcache.lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  
  release(&bcache.lock); */
  /** My Implementation  */
  /** Modify to using time_stamp  */
  int blockno = getHb(b);
  b->time_stamp = ticks;
  if(b->time_stamp == ticks){
    b->refcnt--;
    if(b->refcnt == 0){
      /** 将b脱出  */
      b->next->prev = b->prev;
      b->prev->next = b->next;
      
      /** 将b接入  */
      b->next = bcache.buckets[blockno].next;
      b->prev = &bcache.buckets[blockno];
      bcache.buckets[blockno].next->prev = b;
      bcache.buckets[blockno].next = b;
    }
  }
  //printf("# release blockno: %d \n", blockno);
  
  /* acquire(&bcache.bucketslock[blockno]);
  b->refcnt--;
  if(b->refcnt == 0){
    
    b->next->prev = b->prev;
    b->prev->next = b->next;
    
    
    b->next = bcache.buckets[blockno].next;
    b->prev = &bcache.buckets[blockno];
    bcache.buckets[blockno].next->prev = b;
    bcache.buckets[blockno].next = b;
  }
  release(&bcache.bucketslock[blockno]); */
}

void
bpin(struct buf *b) {
  //printf("see if bpin work\n");
  //int hash = getHb(b);
  b->time_stamp = ticks;
  if(b->time_stamp == ticks)
    b->refcnt++;
  /* acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);  */
  /* acquire(&bcache.bucketslock[hash]);
  b->refcnt++;
  release(&bcache.bucketslock[hash]); */
}

void
bunpin(struct buf *b) {
  //printf("see if bunpin work\n");
  b->time_stamp = ticks;
  if(b->time_stamp == ticks)
    b->refcnt--;
  /* acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);  */
  /* int hash = getHb(b);
  acquire(&bcache.bucketslock[hash]);
  b->refcnt++;
  release(&bcache.bucketslock[hash]); */
}


