/* Copyright (C) 2014 Ram Raghunathan.
 *
 * MLton is released under a BSD-style license.
 * See the file MLton-LICENSE for details.
 */

/**
 * @file local-heap.c
 *
 * @author Ram Raghunathan
 *
 * This file implements the local heap interface defined in
 * local-heap.h.
 */

#include "local-heap.h"

#include "heap-utils.h"

/************************/
/* Function Definitions */
/************************/
void HM_enterLocalHeap (GC_state s) {
  struct HM_HierarchicalHeap* hh = HM_HH_getCurrent(s);

  HM_HH_ensureNotEmpty(hh);
  s->frontier = HM_HH_getFrontier(hh);
  s->limitPlusSlop = HM_HH_getLimit(hh);
  s->limit = s->limitPlusSlop - GC_HEAP_LIMIT_SLOP;
}

void HM_exitLocalHeap (GC_state s) {
  struct HM_HierarchicalHeap* hh = HM_HH_getCurrent(s);

  HM_HH_updateValues(hh, s->frontier);
}

void HM_ensureHierarchicalHeapAssurances(GC_state s,
                                         bool forceGC,
                                         size_t bytesRequested) {
  int processor = Proc_processorNumber (s);
  size_t heapBytesFree = s->limitPlusSlop - s->frontier;

  HM_debugMessage(s,
                  "[%d] HM_ensureHierarchicalHeapAssurances(): bytesRequested: "
                  "%zu, hasHeapBytesFree: %zu\n",
                  processor,
                  bytesRequested,
                  heapBytesFree);

  if (Proc_threadInSection()) {
    HM_debugMessage(s,
                    "[%d] HM_ensureHierarchicalHeapAssurances(): Entering "
                    "Management Heap GC\n",
                    processor);
    HM_enterGlobalHeap();
    ENTER0(s);
    LEAVE0(s);
    HM_exitGlobalHeap();
    HM_debugMessage(s,
                    "[%d] HM_ensureHierarchicalHeapAssurances(): Exiting "
                    "Management Heap GC\n",
                    processor);
  }

  if (!invariantForMutatorStack(s)) {
    /* go to global GC to get the stack invariant satisfied */
    HM_enterGlobalHeap();
    ensureHasHeapBytesFreeAndOrInvariantForMutator (s, forceGC,
                                                    FALSE, TRUE,
                                                    0, 0);
    HM_exitGlobalHeap();
  }

  /* fetch after management heap GC to make sure that I get the updated value */
  struct HM_HierarchicalHeap* hh = HM_HH_getCurrent(s);

  /* update hh before modification */
  HM_HH_updateValues(hh, s->frontier);

  if (s->limitPlusSlop < s->frontier) {
    die(__FILE__ ":%d: s->limitPlusSlop (%p) < s->frontier (%p)",
        __LINE__,
        ((void*)(s->limit)),
        ((void*)(s->frontier)));
  }

  if (((size_t)(s->limitPlusSlop - s->frontier)) < bytesRequested) {
    /* Not enough space, so add new chunk */

    size_t chunkPoolBytesAllocated = ChunkPool_allocated();
    size_t chunkPoolSize = ChunkPool_size();
    double allocatedRatio = ((double)(chunkPoolSize)) /
                            ((double)(chunkPoolBytesAllocated));
    if (allocatedRatio < s->controls->hhRatios.allocatedRatio) {
      /* I may have reached a new maxChunkPoolBytesLive, so check */
      if (s->cumulativeStatistics->maxChunkPoolBytesLive <
          chunkPoolBytesAllocated) {
        s->cumulativeStatistics->maxChunkPoolBytesLive =
            chunkPoolBytesAllocated;
      }

      /* too much allocated, so let's collect */
      HM_HHC_collectLocal();

      chunkPoolBytesAllocated = ChunkPool_allocated();
      chunkPoolSize = ChunkPool_size();
      double newAllocatedRatio = ((double)(chunkPoolSize)) /
                                 ((double)(chunkPoolBytesAllocated));

      LOG(TRUE, FALSE, L_INFO,
          "Live Ratio %.2f < %.2f, performed local collection to increase "
          "ratio to %.2f",
          allocatedRatio,
          s->controls->hhRatios.allocatedRatio,
          newAllocatedRatio);

      ChunkPool_maybeResize();

      /* I may have reached a new maxChunkPoolSize, so check */
      chunkPoolSize = ChunkPool_size();
      if (s->cumulativeStatistics->maxChunkPoolSize < chunkPoolSize) {
        s->cumulativeStatistics->maxChunkPoolSize = chunkPoolSize;
      }
    }

    if (!HM_HH_extend(hh, bytesRequested)) {
      DIE("Ran out of space for Hierarchical Heap!");
    }

    s->frontier = HM_HH_getFrontier(hh);
    s->limitPlusSlop = HM_HH_getLimit(hh);
    s->limit = s->limitPlusSlop - GC_HEAP_LIMIT_SLOP;
  }

  assert(invariantForMutatorFrontier (s));
  assert(invariantForMutatorStack (s));
}
