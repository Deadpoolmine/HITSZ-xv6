// Mutual exclusion lock.
struct spinlock {
  /**
   * 0 means the lock is free
   * non-0 means the lock is held
   */
  uint locked;       // Is the lock held?

  // For debugging:
  char *name;        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
  uint n;
  uint nts;
};

