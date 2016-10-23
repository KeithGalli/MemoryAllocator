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

/*
 * memlib.c - a module that simulates the memory system.  Needed because it
 *            allows us to interleave calls from the student's malloc package
 *            with the system's malloc package in libc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>

#include "./memlib.h"
#include "./config.h"

/* private variables */
static char *mem_start_brk;  /* points to first byte of heap */
static char *mem_brk;        /* points to last byte of heap */
static char *mem_max_addr;   /* largest legal heap address */

/*
 * mem_init - initialize the memory system model
 */
void mem_init(void) {
  /* allocate the storage we will use to model the available VM */
  if ((mem_start_brk = (char *)malloc(MAX_HEAP)) == NULL) {
    fprintf(stderr, "mem_init_vm: malloc error\n");
    exit(1);
  }

  mem_max_addr = mem_start_brk + MAX_HEAP;  /* max legal heap address */
  mem_brk = mem_start_brk;                  /* heap is empty initially */
}

/*
 * mem_deinit - free the storage used by the memory system model
 */
void mem_deinit(void) {
  free(mem_start_brk);
}

/*
 * mem_reset_brk - reset the simulated brk pointer to make an empty heap
 */
void mem_reset_brk(void) {
  mem_brk = mem_start_brk;
}

/*
 * mem_sbrk - simple model of the sbrk function. Extends the heap
 *    by incr bytes and returns the start address of the new area. In
 *    this model, the heap cannot be shrunk.
 */
void *mem_sbrk(int incr) {
  char *old_brk = __sync_fetch_and_add(&mem_brk, incr);

  if ((incr < 0) || (mem_brk > mem_max_addr)) {
    errno = ENOMEM;
    fprintf(stderr, "ERROR: mem_sbrk failed. Ran out of memory... (%ld)\n", mem_heapsize());

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"

    __sync_fetch_and_add(&mem_brk, -incr);

#pragma GCC diagnostic pop

    return (void *)-1;
  }

  return (void *)old_brk;
}

/*
 * mem_heap_lo - return address of the first heap byte
 */
void *mem_heap_lo(void) {
  return (void *)mem_start_brk;
}

/*
 * mem_heap_hi - returns the address of the first byte past the end of the
 *    heap.
 */
void *mem_heap_hi(void) {
  return (void *)(mem_brk - 1);
}

/*
 * mem_heapsize() - returns the heap size in bytes
 */
size_t mem_heapsize(void) {
  return (size_t)(mem_brk - mem_start_brk);
}

/*
 * mem_pagesize() - returns the page size of the system
 */
size_t mem_pagesize(void) {
  return (size_t)getpagesize();
}
