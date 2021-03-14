/* Copyright (C) 2021 Sam Westrick
 *
 * MLton is released under a HPND-style license.
 * See the file MLton-LICENSE for details.
 */

/** Block allocator, inspired by Hoard [1]. Each block is one page.
  *
  * [1] Emery D. Berger, Kathryn S. McKinley, Robert D. Blumofe, and
  * Paul R. Wilson. Hoard: A Scalable Memory Allocator for Multithreaded
  * Applications. ASPLOS 2000.
  */

#ifndef BLOCK_ALLOCATOR_H_
#define BLOCK_ALLOCATOR_H_

#if (defined (MLTON_GC_INTERNAL_TYPES))

struct SuperBlock;

/** Free blocks are used to the store the freelist. */
typedef struct FreeBlock {
  struct FreeBlock *nextFree;
  struct SuperBlock *container;
} *FreeBlock;


struct BlockAllocator;


typedef struct SuperBlock {

  struct BlockAllocator *owner;

  /** Superblocks only allocate in groups of 2^sizeClass contiguous blocks.
    * E.g. sizeClass == 2 means that blocks are allocated in groups of 4:
    *   {1 2 3 4}  {5 6 7 8}  {9 10 11 12} ...
    * In the freeFlags and freeList, the only block-ids you will see are
    * the starting block-ids (1, 5, 9, etc in the above example)
    */
  int sizeClass;

  /** Number of blocks in this superblock that are currently free. For example
    * When we perform an alloction in this superblock, we do:
    *   numBlocksFree -= 2^sizeClass;
    */
  int numBlocksFree;

  /** The freelist is LIFO, for caching/efficiency. */
  FreeBlock firstFree;

  /** The barrier between free blocks and unused space in the rest of the
    * superblock. The frontier will always be aligned at a valid start for
    * a freeblock, according to the size-class. The benefit of doing it
    * this way is that we can change the size-class of a completely empty
    * superblock in O(1)... it doesn't depend on the superblock size.
    */
  pointer frontier;

  /** Each fullness group within a size-class is a doubly-linked list. */
  struct SuperBlock *nextSuperBlock;
  struct SuperBlock *prevSuperBlock;

  uint32_t magic;

} *SuperBlock;


typedef struct SuperBlockList {

  SuperBlock firstSuperBlock;

} *SuperBlockList;


/** num groups is one less, because we handle COMPLETELY_EMPTY specially. */
#define NUM_FULLNESS_GROUPS 4
enum FullnessGroup {
  COMPLETELY_FULL = 0,
  NEARLY_FULL = 1,
  SOMEWHAT_FULL = 2,
  NEARLY_EMPTY = 3,
  COMPLETELY_EMPTY = 4
};


typedef struct BlockAllocator {

  /** These are used for local to decide to move things to global, but
    * are ignored in global.
    */
  size_t numBlocks;
  size_t numBlocksInUse;

  /** There are 3 fullness groups in each size class:
    *   0 is completely full, i.e. no free blocks available
    *   1 is nearly full, i.e. at least 1-emptinessFraction in use
    *   2 is neither nearly full nor nearly empty
    *   3 is nearly empty, i.e. less than emptinessFraction in use
    */
  struct SuperBlockList *sizeClassFullnessGroup;

  /** Completely empty superblocks are special because these can be
    * reused for any size class.
    */
  struct SuperBlockList completelyEmptyGroup;

  /** Concurrent freelist (blocks owned by this proc that were freed by some
    * other proc). To make the concurrency simpler, these blocks are enqueued
    * for this proc and then this proc may free them at its convenience.
    */
  FreeBlock firstFreedByOther;

} *BlockAllocator;


typedef struct Blocks {
  SuperBlock container;
  size_t numBlocks;
} *Blocks;

#else

struct BlockAllocator;
typedef struct BlockAllocator * BlockAllocator;

#endif // MLTON_GC_INTERNAL_TYPES



#if (defined (MLTON_GC_INTERNAL_FUNCS))

BlockAllocator initGlobalBlockAllocator(GC_state s);
void initLocalBlockAllocator(GC_state s, BlockAllocator globalAllocator);

/** Get a pointer to the start of some number of free contiguous blocks. */
Blocks allocateBlocks(GC_state s, size_t numBlocks);

/** Free a group of contiguous blocks. */
void freeBlocks(GC_state s, Blocks bs);

#endif

#endif // BLOCK_ALLOCATOR_H_
