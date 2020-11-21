struct buf {
  int valid;   // has data been read from disk? 
               // The field valid indicates that the buffer contains a copy of the block
  int disk;    // does disk "own" buf? 
               // The field disk indicates that the buffer content has been handed to the
               // disk, which may change the buffer (e.g., write data from the disk into data).
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  struct buf *prev; // LRU cache list
  struct buf *next;
  uchar data[BSIZE];
  /** 尝试使用time_stamp */
  uint time_stamp; 
  
};

