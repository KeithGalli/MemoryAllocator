 /*
 * validator.h - 6.172 Malloc Validator
 *
 * Validates a malloc/free/realloc implementation.
 *
 * Copyright (c) 2010, R. Bryant and D. O'Hallaron, All rights reserved.
 * May not be used, modified, or copied without permission.
 */

#ifndef MM_VALIDATOR_H
#define MM_VALIDATOR_H

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>

#include "./config.h"
#include "./memlib.h"

// Returns true if p is R_ALIGNMENT-byte aligned
#if (__WORDSIZE == 64 )
#define IS_ALIGNED(p)  ((((uint64_t)(p)) % R_ALIGNMENT) == 0)
#else
#define IS_ALIGNED(p)  ((((uint32_t)(p)) % R_ALIGNMENT) == 0)
#endif

// Range list data structure

// Records the extent of each block's payload
typedef struct range_t {
  char *lo;              // low payload address
  char *hi;              // high payload address
  struct range_t *next;  // next list element
} range_t;

// The following routines manipulate the range list, which keeps
// track of the extent of every allocated block payload. We use the
// range list to detect any overlapping allocated blocks.


// add_range - As directed by request opnum in trace tracenum,
// we've just called the student's malloc to allocate a block of
// size bytes at addr lo. After checking the block for correctness,
// we create a range struct for this block and add it to the range list.
static int add_range(const malloc_impl_t *impl, range_t **ranges, char *lo,
    int size, int tracenum, int opnum) {
  char *hi = lo + size - 1;
  range_t *p;
  range_t *pnext;

  // You can use this as a buffer for writing messages with sprintf.
  // char msg[MAXLINE];

  assert(size > 0);

  // Payload addresses must be R_ALIGNMENT-byte aligned
  if (!IS_ALIGNED(lo)) {
    printf("Not Aligned!\n");
    return 0;
  }
  
  // The payload must lie within the extent of the heap
  if (hi > (char *)my_heap_hi()) {
    printf("Payload not in heap\n");
    return 0;
  }

  
  // The payload must not overlap any other payloads
  for (p = *ranges; p != NULL; p = pnext) {
    pnext = p->next;
    if ((p->hi >= lo && p->lo <= hi) || (p->lo <= hi && p->hi >= lo)) {
      printf("Overlapping payloads\n");
      return 0;
    }
  }


  // Everything looks OK, so remember the extent of this block by creating a
  // range struct and adding it the range list. 
  range_t *new_range = malloc(sizeof(range_t));
  new_range->lo = lo;
  new_range->hi = hi;
  new_range->next = *ranges;
  *ranges = new_range;
 
  return 1;
}

// remove_range - Free the range record of block whose payload starts at lo
static void remove_range(range_t **ranges, char *lo) {
  range_t *p = NULL;
  range_t *p2 = NULL;
  range_t **prevpp = ranges;

  // Iterate the linked list until you find the range with a matching lo
  // payload and remove it.  Remember to properly handle the case where the
  // payload is in the first node, and to free the node after unlinking
  if ((*prevpp)->lo == lo) {
    p = (*prevpp)->next;
    free(*prevpp);
    *ranges = p;
  } else {
    p = *prevpp;
    p2 = (*prevpp)->next;
    while (p2->lo != lo) {
      p = p->next;
      p2 = p2->next;
    } 
    p->next = p2->next;
    free(p2);
  }      
}

// clear_ranges - free all of the range records for a trace
static void clear_ranges(range_t **ranges) {
  range_t *p;
  range_t *pnext;

  for (p = *ranges; p != NULL; p = pnext) {
    pnext = p->next;
    free(p);
  }
  *ranges = NULL;
}

// eval_mm_valid - Check the malloc package for correctness
int eval_mm_valid(const malloc_impl_t *impl, trace_t *trace, int tracenum) {
  int i = 0;
  int index = 0;
  int size = 0;
  int oldsize = 0;
  char *newp = NULL;
  char *oldp = NULL;
  char *p = NULL;
  range_t *ranges = NULL;

  // Reset the heap.
  impl->reset_brk();

  // Call the mm package's init function
  if (impl->init() < 0) {
    malloc_error(tracenum, 0, "impl init failed.");
    return 0;
  }

  // Interpret each operation in the trace in order
  for (i = 0; i < trace->num_ops; i++) {
    index = trace->ops[i].index;
    size = trace->ops[i].size;

    switch (trace->ops[i].type) {
      case ALLOC:  // malloc

        // Call the student's malloc
        if ((p = (char *) impl->malloc(size)) == NULL) {
          malloc_error(tracenum, i, "impl malloc failed.");
          return 0;
        }

        // Test the range of the new block for correctness and add it
        // to the range list if OK. The block must be  be aligned properly,
        // and must not overlap any currently allocated block.
        if (add_range(impl, &ranges, p, size, tracenum, i) == 0)
          return 0;

        // Fill the allocated region with some unique data that you can check
        // for if the region is copied via realloc.
        char first_check = (char)index;

        for (int j = 0; j < size; j++) {
          *((char *)p + j) = first_check;
        }

        // Remember region
        trace->blocks[index] = p;
        trace->block_sizes[index] = size;
        break;

      case REALLOC:  // realloc

        // Call the student's realloc
        oldp = trace->blocks[index];
        if ((newp = (char *) impl->realloc(oldp, size)) == NULL) {
          malloc_error(tracenum, i, "impl realloc failed.");
          return 0;
        }

        // Remove the old region from the range list
        remove_range(&ranges, oldp);

        // Check new block for correctness and add it to range list
        if (add_range(impl, &ranges, newp, size, tracenum, i) == 0)
          return 0;

        // Make sure that the new block contains the data from the old block,
        // and then fill in the new block with new data that you can use to
        // verify the block was copied if it is resized again.
        oldsize = trace->block_sizes[index];
        if (size < oldsize)
          oldsize = size;

        // Make sure that the oldp matches the newp
        for (int j = 0; j < oldsize; j++) {
          if (*((char *)newp + j) != (char)index) {
            printf("invalid data copied");
            return 0;
          }
        }
        
        // Put the new data in place to make sure the value was copied properly

        char second_check = (char)index;
        for (int k=0; k < size; k++) {
          *((char *)newp + k) = second_check;
        }
 
        // Remember region
        trace->blocks[index] = newp;
        trace->block_sizes[index] = size;
        break;

      case FREE:  // free

        // Remove region from list and call student's free function
        p = trace->blocks[index];
        remove_range(&ranges, p);
        impl->free(p);
        break;

      case WRITE:  // write

        break;

      default:
        app_error("Nonexistent request type in eval_mm_valid");
    }
  }

  // Free ranges allocated and reset the heap.
  impl->reset_brk();
  clear_ranges(&ranges);

  // As far as we know, this is a valid malloc package
  return 1;
}
#endif  // MM_VALIDATOR_H
