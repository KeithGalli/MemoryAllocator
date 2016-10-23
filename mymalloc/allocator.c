/**
 * Copyright (c) 2015 MIT License by 6.172 Staff
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 **/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "./allocator_interface.h"
#include "./memlib.h"

// Don't call libc malloc!
#define malloc(...) (USE_MY_MALLOC)
#define free(...) (USE_MY_FREE)
#define realloc(...) (USE_MY_REALLOC)

// All blocks must have a specified minimum alignment.
// The alignment requirement (from config.h) is >= 8 bytes.
#ifndef ALIGNMENT
#define ALIGNMENT 8
#endif

// Built in function that can find the offsetof a struct to one of its fields
#define offsetof(type, member)  __builtin_offsetof (type, member)

// Built in function that calculates the number of zero_bits on the left of a number
#define left_zeros(size) __builtin_clz (size)

// Rounds up to the nearest multiple of ALIGNMENT.
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

// The total number of free_lists we'll use
#define LIST_SIZE 25

typedef struct header_t { 
  size_t size;
  struct header_t * next;
  struct header_t * prev;
} header_t;

#define HEADER_T_SIZE ALIGN(sizeof(header_t))

typedef struct footer_t {
  size_t size;
} footer_t;

#define FOOTER_T_SIZE ALIGN(sizeof(footer_t))

#define FREE_HEADER_SIZE HEADER_T_SIZE-offsetof(header_t, next)

// We will use the very last bit of a 64-bit number to
// represent whether a block is free or not. Because we know
// that this is 8-byte aligned, we know that this bit will
// always be zero, so we can use it to store a bit of info.
#define FREE_BIT 0x0000000000000001

// Returns 1 if chunk is free, 0 otherwise
#define is_free(chunk) ((chunk)->size & FREE_BIT) 

// Set's the new size of a chunk, keeping it's in use status the same
#define set_size(new_size, chunk) ((chunk)->size = (new_size)|(is_free(chunk))) 

// Get the size of a specific chunk of memory, masking out the free_bit
#define get_size(chunk) ((chunk)->size & ~FREE_BIT)

// This block of memory is now free, mark it appropriately
#define set_free(chunk) ((chunk)->size |= FREE_BIT)

// This block of memory is in use, mark it appropriately
#define set_in_use(chunk) ((chunk)->size &= ~FREE_BIT)

// This represents the minimum size we should split at (tunable value)
#define SPLIT_CONSTANT 112

// This represents the number of blocks we should check after we find a free block fit
// to see if we can find a better fit (tunable value)
#define BEST_CONSTANT 4

int free_list_max;

header_t * free_lists[LIST_SIZE]; 

// Method finds the appropriate free_list index for a given size
static inline int calculate_hash(const size_t size);

// Calculate MSB for a specific size
static inline int calculate_hash(const size_t size);

// If you are allocating a size that is less than it's container, add the difference to a free_list bin
void * free_remaining_memory(const void * p, const size_t free_list_size, const size_t aligned_size);

// Merge to free lists together to create a larger chunk of free memory
header_t * coalesce(const void * ptr);

// This free_list_addresss is no longer free/ or has a different size. Remove it from the appropriate bin
void remove_free_list_address(header_t * hdr_ptr);

// Once we find a block of memory that fits what we need, check a couple more bins to see if we can find a better fit
header_t * get_best_block(const size_t size, header_t * best_block);

bool free_availible;

// check - This checks our invariant that the size_t header before every
// block points to either the beginning of the next block, or the end of the
// heap.
int my_check() {
  char *p;
  char *lo = (char*)mem_heap_lo();
  char *hi = (char*)mem_heap_hi() + 1;
  size_t size = 0;
  size_t p_size;

  p = lo;
  while (lo <= p && p < hi) {
    header_t * header = (header_t *)p;
    p_size = get_size(header);
    size = p_size + offsetof(header_t, next) + FOOTER_T_SIZE; 
    p += size;
  }

  if (p != hi) {
    printf("Bad headers did not end at heap_hi!\n");
    printf("heap_lo: %p, heap_hi: %p, size: %lu, p: %p\n", lo, hi, size, p);
    return -1;
  }

  return 0;
}

// init - Initialize the malloc package.  Called once before any other
// calls are made.  Since this is a very simple implementation, we just
// return success.
inline int my_init() {
  // Set all of the free_list HEADS to NULL initially
  for (int i=0; i < LIST_SIZE; i++) {
    free_lists[i] = NULL;
  }
  free_list_max = 0;
  return 0;
}

inline void * my_allocator(const size_t size) {
  // Expands the heap by the given number of bytes and returns a pointer to
  // the newly-allocated area. This is a slow call, so you will want to
  // make sure you don't wind up calling it on every malloc
  void *p = mem_sbrk(size);

  if (p == (void *)-1) {
    // Some sort of error occurred. We return NULL to let
    // the client code know that we weren't able to allocate memory
    return NULL;
  }
  return p;
}

//  malloc - Allocate a block by incrementing the brk pointer.
//  Always allocate a block whose size is a multiple of the alignment.
inline void * my_malloc(const size_t size) {
  // We allocate a little bit of extra memory so that we can store the
  // size of the block we've allocated.  Take a look at realloc to see
  // one example of a place where this can come in handy.
  size_t stored_size = ALIGN(size);
  if (size < FREE_HEADER_SIZE) {
     // To ensure our allocation doesn't break, allocate a little extra space if size < FREE_LIST_SIZE
     stored_size = FREE_HEADER_SIZE;
  }
  const size_t aligned_size = ALIGN(stored_size + offsetof(header_t, next) + FOOTER_T_SIZE);

  const int sig_bit = calculate_hash(stored_size);
  const int allocation_power = sig_bit + 1; // Allocate the power of two that is just greater than our size
  
  assert(allocation_power < LIST_SIZE);
  
  void * p = NULL;
  header_t * header;
  footer_t * footer;

  // Linear search the free_lists
  if (free_lists[sig_bit] != NULL) {
    // Check to see if the first block in the appropriate free_list can fit the block we want to allocate
    if (get_size(free_lists[sig_bit]) >= stored_size) {
      header_t * best_block = get_best_block(stored_size, free_lists[sig_bit]);
      p = (void *)(best_block);
      stored_size = get_size(best_block);
      remove_free_list_address(best_block);
    } else {
      // Iterate through the linked list of the appropriate size to see if we can find a block big enough for us to allocate too.
      header_t * free_pointer = free_lists[sig_bit];
      header_t * free_pointer2 = free_lists[sig_bit]->next;
      while (free_pointer2 != NULL) {
        if (get_size(free_pointer2) >= stored_size) {
          // If this condition is met, you have found a free list spot to allocate to
          header_t * best_block = get_best_block(stored_size, free_pointer2);
          p = (void *)(best_block); 
          stored_size = get_size(best_block);
          remove_free_list_address(best_block);
          break;
        }
        free_pointer = free_pointer->next;
        free_pointer2 = free_pointer2->next;
      }
    }
  } 
 
  size_t free_list_size;
  // If we didn't find anything in our linear search, look at the larger bins
  if (p == NULL) {
    for (int i = allocation_power; i <= free_list_max; i++) {
      // Check to see if there is any blocks of memory in this free list
      if (free_lists[i] != NULL) {
        // Find a good fitting block for this size
        header_t * best_block = get_best_block(stored_size, free_lists[i]);
        p = (void *)(best_block);
        // Check to see if you have a good amount of extra memory. If you do, add the extra memory to a seperate free memory bin.
        if ((aligned_size <= get_size(best_block)) && (get_size(best_block) - aligned_size) >= FREE_HEADER_SIZE + SPLIT_CONSTANT) {
          assert(aligned_size <= get_size(best_block));
          free_list_size = get_size(best_block);
          remove_free_list_address(p);

          header = (header_t *)p;
          set_size(stored_size, header);
          set_in_use(header);
          footer = (footer_t *)((uint8_t *) p + offsetof(header_t, next) + stored_size);
          footer->size = stored_size;
          // assert(header->size == ((footer_t *)((uint8_t *)header + offsetof(header_t, next) + header->size))->size);
          free_remaining_memory(p, free_list_size, aligned_size);
        } else { //This block is a pretty tight fit, just use all of it
          stored_size = get_size(best_block);
          remove_free_list_address(p);
        } // Could freeing extra memory mess with this next call?
        break;
      }
    }
    // If this condition is met, we couldn't find an appropriate free spot. Call my_allocator
    // To make the call to mem_sbrk to get additional memory
    if (p == NULL) {
      p = my_allocator(aligned_size);
      // None of our allocation methods were successful. Return NULL as a result
      if (p == NULL) {
        return NULL;
      }
    }
  }
/*  
  // If this condition is met, we couldn't find an appropriate free spot. Call my_allocator
  // To make the call to mem_sbrk to get additional memory
  if (p == NULL) { 
    p = my_allocator(aligned_size);
    // None of our allocation methods were successful. Return NULL as a result
    if (p == NULL) {
      return NULL;
    }
  }
*/
  // None of our allocation methods were successful. Return NULL as a result
//  if (p == NULL) {
//    return NULL;
//  }

  // Set the appropriate new header values calculated above
  header = (header_t *)p;
  set_size(stored_size, header);
  set_in_use(header);
  
  // Set the appropriate new footer values calculated above
  footer = (footer_t *)((uint8_t *) p + offsetof(header_t, next) + stored_size);
  footer->size = stored_size;
  
  assert(get_size(header) == footer->size);
  assert(get_size(header) == ((footer_t *)((uint8_t *)header + offsetof(header_t, next) + stored_size))->size);
  // Then, we return a pointer to the rest of the block of memory,
  // which is at least size bytes long.  We have to cast to uint8_t
  // before we try any pointer arithmetic because voids have no size
  // and so the compiler doesn't know how far to move the pointer.
  // Since a uint8_t is always one byte, adding SIZE_T_SIZE after
  // casting advances the pointer by SIZE_T_SIZE bytes.
  return (void *)((uint8_t *)p + offsetof(header_t, next));
}

// free the block of memory at address void* ptr. This method checks the size of the block we want to free 
// and calculates its hash so that it can go into the proper ranged bin
void my_free(void *ptr) {
  header_t * header = (header_t *)((uint8_t *)ptr - offsetof(header_t, next));
  assert(is_free(header) == false);
  assert(get_size(header) > 0);
  assert(get_size(header) == ((footer_t *)((uint8_t *)header + offsetof(header_t, next) + get_size(header)))->size);

  header = coalesce(ptr);
  size_t size = get_size(header);
  assert(size == ALIGN(size));
  size_t stored_memory = ALIGN(size);
  size_t sig_bit = calculate_hash(stored_memory); // Get the most significant bit of the amount of memory we stored
  
  assert(sig_bit < LIST_SIZE);
  header->prev = NULL;
  header->next = free_lists[sig_bit]; // Store free space in proper ranged_bin
  if (free_lists[sig_bit] != NULL) {
    free_lists[sig_bit]->prev = header;
  }
  set_free(header);
  free_lists[sig_bit] = header;
  assert(get_size(header) == ((footer_t *)((uint8_t *)header + offsetof(header_t, next) + size))->size);
  if (sig_bit > free_list_max) {
    free_list_max = sig_bit;
  }
}

// realloc - The overall method just makes use of my_malloc and my_free
// Special cases: 
// - size < ptr->size. The block can just be kept in place
// - reallocing last block, just expand the end of the heap the amount you need to,
// instead of trying to find a completely new spot to store the block
void * my_realloc(void *ptr, size_t size) {
  if (ptr == NULL) {
    return my_malloc(size);
  } else if (size == 0) {
    my_free(ptr);
    return NULL;
  }
  
  void *newptr;
  size_t copy_size;

  // Allocate a new chunk of memory, and fail if that allocation fails.
  header_t * header = (header_t *)((uint8_t*)ptr - offsetof(header_t, next));
 
  size_t new_size = ALIGN(size);

  // Get the size of the old block of memory.  Take a peek at my_malloc(),
  // where we stashed this in the SIZE_T_SIZE bytes directly before the
  // address we returned.  Now we can back up by that many bytes and read
  // the size.
  copy_size = get_size(header);
 
  // If the new block is smaller than the old one, we have to stop copying
  // early so that we don't write off the end of the new block of memory.
  if (size < copy_size) {
    //TODO FREE REMAINING MEMORY
    return ptr;     
  }

  size_t difference = new_size - copy_size;
  void * right_most = (void *)((uint8_t *)header + offsetof(header_t, next) + copy_size + FOOTER_T_SIZE);
  if (right_most == (void *)(mem_heap_hi() + 1)) { //This is the last block in the heap
    mem_sbrk(difference);
    set_size(new_size, header);
    return ptr;
  }

  newptr = my_malloc(size);
  if (NULL == newptr)
    return NULL;

  // This is a standard library call that performs a simple memory copy.
  memcpy(newptr, ptr, copy_size);

  // Release the old block.
  my_free(ptr);

  // Return a pointer to the new block.
  return newptr;
}

// call mem_reset_brk.
inline void my_reset_brk() {
  mem_reset_brk();
}

// call mem_heap_lo
inline void * my_heap_lo() {
  return mem_heap_lo();
}

// call mem_heap_hi
inline void * my_heap_hi() {
  return mem_heap_hi();
}

// Use the gcc built in function (defined as left_zeros) to help us calculate the log2
static inline int calculate_hash(const size_t size) {
  const int sig_bit = 31 - left_zeros(size);
  return sig_bit; 
}

inline void * free_remaining_memory(const void * p, const size_t free_list_size, const size_t aligned_size) {
  header_t * free_block = (header_t *)((uint8_t *)p + aligned_size);
  size_t free_block_size = free_list_size - aligned_size;
  footer_t * free_block_footer = (footer_t *)((uint8_t *)free_block + offsetof(header_t, next) + free_block_size);

  assert(free_block_size >= FREE_HEADER_SIZE);
  assert((uint8_t *) free_block > (uint8_t *)p);

  set_size(free_block_size, free_block);
  free_block_footer->size = free_block_size;
  set_in_use(free_block);
  assert(get_size(free_block) == ((footer_t *)((uint8_t *)free_block + offsetof(header_t, next) + free_block->size))->size);

  my_free((void*)((uint8_t *)free_block + offsetof(header_t, next)));
  return NULL;
}

inline header_t * coalesce(const void * ptr) {
  header_t * header = (header_t *)((uint8_t *)ptr - offsetof(header_t, next));
  footer_t * footer = (footer_t *)((uint8_t *)ptr + get_size(header));
  assert(get_size(header) == ((footer_t *)((uint8_t *)header + offsetof(header_t, next) + get_size(header)))->size);  
  bool is_left_free = false;
  footer_t * left_foot;
  size_t left_size;
  header_t * left_header;

  if ((char *)header > (char *)my_heap_lo()) {
    left_foot = (footer_t *)((uint8_t *)header - FOOTER_T_SIZE);
    left_size = left_foot->size;
    left_header = (header_t *)((uint8_t *)left_foot - left_size - offsetof(header_t, next));
    is_left_free = is_free(left_header);
    assert(left_size == get_size(left_header));
    assert(left_size >= FREE_HEADER_SIZE);
  }

  bool is_right_free = false;
  header_t * right_header;
  size_t right_size;
  footer_t * right_footer;
 
  if ((char *)footer + FOOTER_T_SIZE < (char *)my_heap_hi() + 1) {
    right_header = (header_t *)((uint8_t *)header + offsetof(header_t, next) + get_size(header) + FOOTER_T_SIZE);
    is_right_free = is_free(right_header);
    
    assert(get_size(header) == ((footer_t *)((uint8_t *)header + offsetof(header_t, next) + get_size(header)))->size);
//    assert(right_size == right_footer->size);
//    assert(right_size >= FREE_HEADER_SIZE);
  }

  header_t * new_header = header;
  footer_t * new_footer = footer;
  size_t new_size = get_size(header);
  
  if (is_left_free) {
    // Remove left from it's current free_list
    remove_free_list_address(left_header);

    // Change the size appropriately 
    new_size += left_size + FOOTER_T_SIZE + offsetof(header_t, next);

    // Update the free_list values
    new_header = left_header;
  } 

  if (is_right_free) {
    right_size = get_size(right_header);
    right_footer = (footer_t *)((uint8_t *)right_header + offsetof(header_t, next) + right_size);

    // Remove right from it's current free_list
    remove_free_list_address(right_header);
    
    // Change the size appropriately
    new_size += FOOTER_T_SIZE + offsetof(header_t, next) + right_size;

    new_footer = right_footer;
  }

  // Update the free_list values
  set_in_use(new_header);
  set_size(new_size, new_header);
  new_footer->size = new_size;
  assert(get_size(new_header) == ((footer_t *)((uint8_t *)new_header + offsetof(header_t, next) + new_header->size))->size);
  return new_header;  
}

inline void remove_free_list_address(header_t * hdr_ptr) {
  size_t size;
  size_t hash;

  if (hdr_ptr->prev == NULL) {
    size = get_size(hdr_ptr);
    hash = calculate_hash(size);
    free_lists[hash] = hdr_ptr->next;
  } else {
    (hdr_ptr->prev)->next = hdr_ptr->next;
  }
  
  if (hdr_ptr->next != NULL) {
    (hdr_ptr->next)->prev = hdr_ptr->prev;
  }
}

inline header_t * get_best_block(const size_t size, header_t * best_block) {
  header_t * test_block = best_block->next;
  size_t best_size = get_size(best_block);
  int count = 0;
  while ((count < BEST_CONSTANT) && test_block != NULL) {
    count += 1;
    size_t test_size = get_size(test_block);
    if (test_size >= size && test_size < best_size) {
      best_block = test_block;
      best_size = test_size;
    }
    test_block = test_block->next;
  }
  return best_block;
}
