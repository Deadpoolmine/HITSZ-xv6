// On-disk file system format.
// Both the kernel and user programs use this header file.


#define ROOTINO  1   // root i-number
#define BSIZE 1024  // block size

// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
/**
 * 
 * super block contains metadata about 
 * the file system (the file system size in blocks, 
 * the number of data blocks, the number of inodes,
 * and the number of blocks in the log)
 * 
 * The log resides at a known fixed location, specified in the superblock 
 */
struct superblock {
  uint magic;        // Must be FSMAGIC
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
  /** log，用以判断crash后是否恢复  */
  uint nlog;         // Number of log blocks
  uint logstart;     // Block number of first log block
  uint inodestart;   // Block number of first inode block
  uint bmapstart;    // Block number of first free map block
};

#define FSMAGIC 0x10203040

/** direct blocks */
#define NDIRECT 11
/** indirect blocks */
#define SINGLEDIRECT (BSIZE / sizeof(uint))
#define NINDIRECT (SINGLEDIRECT + SINGLEDIRECT * BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)
/**
 * 
 * The inode layer provides individual files, each 
 * represented as an inode with a unique i-number and some blocks 
 * holding the file’s data
 */

/**
 * It might refer to the on-disk data structure
   containing a file’s size and list of data block numbers. Or “inode” might refer to an in-memory
   inode, which contains a copy of the on-disk inode as well as extra information needed within the
   kernel.
 */
// On-disk inode structure
struct dinode {
  short type;           // File type 
  short major;          // Major device number (T_DEVICE only)
  short minor;          // Minor device number (T_DEVICE only)
  /** 
   * The nlink field counts the number of directory
   * entries that refer to this inode,
   * in order to recognize when the on-disk 
   * inode and its data blocks should be freed.   
   * */
  short nlink;          // Number of links to inode in file system
  /**
   * The size field records the number of bytes of content in the file.
   */
  uint size;            // Size of file (bytes)
  /**
   * The addrs array records the block numbers of 
   * the disk blocks holding the file’s content.
   */

  /** Modify NDIRENT to 11， and implements SINGLE-INDIRENT & DOUBLE-INDIRENT  */
  uint addrs[NDIRECT+2];   // Data block addresses
};

// Inodes per block.
#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

// Bitmap bits per block
#define BPB           (BSIZE*8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) ((b)/BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

