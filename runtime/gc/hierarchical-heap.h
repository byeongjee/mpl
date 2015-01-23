/* Copyright (C) 2014,2015 Ram Raghunathan.
 *
 * MLton is released under a BSD-style license.
 * See the file MLton-LICENSE for details.
 */

/**
 * @file hierarchical-heap.h
 *
 * @author Ram Raghunathan
 *
 * @brief
 * Definition of the HierarchicalHeap object and management interface
 */

#ifndef HIERARCHICAL_HEAP_H_
#define HIERARCHICAL_HEAP_H_

#if (defined (MLTON_GC_INTERNAL_TYPES))
/**
 * @brief
 * Represents a "node" of the hierarchical heap and contains various data
 * about its contents and structure
 *
 * HierarchicalHeap objects are normal objects with the following layout:
 *
 * header ::
 * padding ::
 * lastAllocatedChunk (void*) ::
 * savedFrontier (void*) ::
 * chunkList (void*) ::
 * parentHH (objptr) ::
 * nextChildHH (objptr) ::
 * childHHList (objptr)
 *
 * There may be zero or more bytes of padding for alignment purposes.
 */
struct HM_HierarchicalHeap {
  void* lastAllocatedChunk; /**< The last allocated chunk, so that 'chunkList'
                             * does not have to be traversed to find it. */

  void* savedFrontier; /**< The saved frontier when returning to this
                        * hierarchical heap. */

  void* chunkList; /**< The list of chunks making up this heap. */

  objptr parentHH; /**< The heap this object branched off of or BOGUS_OBJPTR
                    * if it is the first heap. */

  objptr nextChildHH; /**< The next heap in the 'derivedHHList' of
                       * the 'parentHH'. This variable is the 'next'
                       * pointer for the intrusive linked list. */

  objptr childHHList; /**< The list of heaps that are derived from this
                       * heap. All heaps in this list should have their
                       * 'parentHH' set to this object. */
} __attribute__((packed));

COMPILE_TIME_ASSERT(HM_HierarchicalHeap__packed,
                    sizeof (struct HM_HierarchicalHeap) ==
                    sizeof (void*) +
                    sizeof (void*) +
                    sizeof (void*) +
                    sizeof (objptr) +
                    sizeof (objptr) +
                    sizeof (objptr));
#else
struct HM_HierarchicalHeap;
#endif /* MLTON_GC_INTERNAL_TYPES */

#if (defined (MLTON_GC_INTERNAL_FUNCS))
/* RAM_NOTE: should take GC_state argument once I get that back in */

/**
 * Pretty-prints the hierarchical heap structure
 *
 * @param hh The struct HM_HierarchicalHeap to print
 * @param stream The stream to print to
 */
static inline void HM_displayHierarchicalHeap(
    const struct HM_HierarchicalHeap* hh,
    FILE* stream);

/**
 * Returns the sizeof the the struct HM_HierarchicalHeap in the heap including
 * header and padding
 *
 * @param s The GC_state to take alignment off of
 *
 * @return total size of the struct HM_HierarchicalHeap object
 */
static inline size_t HM_sizeofHierarchicalHeap(GC_state s);

/**
 * Returns offset into object to get at the struct HM_HierarchicalHeap
 *
 * @note
 * Objects are passed to runtime functions immediately <em>after</em> the
 * header, so we only need to pass the padding to get to the struct.
 *
 * @param s The GC_state to get alignment off of for HM_sizeofHierarchicalHeap()
 *
 * @return The offset into the object to get to the struct HM_HierarchicalHeap
 */
static inline size_t HM_offsetofHierarchicalHeap(GC_state s);
#endif /* MLTON_GC_INTERNAL_FUNCS */

/**
 * Appends the derived hierarchical heap to the childHHList of the source
 * hierarchical heap and sets relationships appropriately.
 *
 * @attention
 * 'childHH' should be a newly created "orphan" HierarchicalHeap.
 *
 * @param parentHH The source struct HM_HierarchicalHeap
 * @param childHH The struct HM_HierarchicalHeap set to be derived off of
 * 'parentHH'.
 */
PRIVATE void HM_appendChildHierarchicalHeap(pointer parentHHObject,
                                             pointer childHHObject);

/**
 * Merges the specified hierarchical heap back into its source hierarchical
 * heap.
 *
 * @attention
 * The specified heap must already be fully merged (i.e. its childHHList should
 * be empty).
 *
 * @attention
 * Once this function completes, the passed-in heap should be considered used
 * and all references to it dropped.
 *
 * @param hh The struct HM_HierarchicalHeap* to merge back into its source.
 */
PRIVATE void HM_mergeIntoParentHierarchicalHeap(pointer hhObject);

#if ASSERT
/**
 * Asserts all of the invariants assumed for the struct HM_HierarchicalHeap.
 *
 * @attention
 * If an assertion fails, this function aborts the program, as per the assert()
 * macro.
 *
 * @param hh The struct HM_HierarchicalHeap to assert invariants for
 */
void HM_assertHierarchicalHeapInvariants(const struct HM_HierarchicalHeap* hh);
#endif /* ASSERT */

#endif /* HIERARCHICAL_HEAP_H_ */