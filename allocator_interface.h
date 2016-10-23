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

#include <assert.h>
#include <stdlib.h>

#ifndef _ALLOCATOR_INTERFACE_H
#define _ALLOCATOR_INTERFACE_H

/* Function pointers for a malloc implementation.  This is used to allow a
 * single validator to operate on both libc malloc, a buggy malloc, and the
 * student "mm" malloc.
 */
typedef struct {
  int (*init)(void);
  void *(*malloc)(size_t size);
  void *(*realloc)(void *ptr, size_t size);
  void (*free)(void *ptr);
  int (*check)();
  void (*reset_brk)(void);
  void *(*heap_lo)(void);
  void *(*heap_hi)(void);
} malloc_impl_t;

int libc_init();
void * libc_malloc(size_t size);
void * libc_realloc(void *ptr, size_t size);
void libc_free(void *ptr);
int libc_check();
void libc_reset_brk();
void * libc_heap_lo();
void * libc_heap_hi();

static const malloc_impl_t libc_impl =
{ .init = &libc_init, .malloc = &libc_malloc, .realloc = &libc_realloc,
  .free = &libc_free, .check = &libc_check, .reset_brk = &libc_reset_brk,
  .heap_lo = &libc_heap_lo, .heap_hi = &libc_heap_hi};

int my_init();
void * my_malloc(size_t size);
void * my_realloc(void *ptr, size_t size);
void my_free(void *ptr);
int my_check();
void my_reset_brk();
void * my_heap_lo();
void * my_heap_hi();

static const malloc_impl_t my_impl =
{ .init = &my_init, .malloc = &my_malloc, .realloc = &my_realloc,
  .free = &my_free, .check = &my_check, .reset_brk = &my_reset_brk,
  .heap_lo = &my_heap_lo, .heap_hi = &my_heap_hi};

int bad_init();
void * bad_malloc(size_t size);
void * bad_realloc(void *ptr, size_t size);
void bad_free(void *ptr);
int bad_check();
void bad_reset_brk();
void * bad_heap_lo();
void * bad_heap_hi();

static const malloc_impl_t bad_impl =
{ .init = &bad_init, .malloc = &bad_malloc, .realloc = &bad_realloc,
  .free = &bad_free, .check = &bad_check, .reset_brk = &bad_reset_brk,
  .heap_lo = &bad_heap_lo, .heap_hi = &bad_heap_hi};

#endif  // _ALLOCATOR_INTERFACE_H
