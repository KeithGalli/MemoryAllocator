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

#include "./allocator_interface.h"

/* Libc needs no initialization. */
int libc_init() {
  return 0;
}

/* Libc has no heap checker, so we just return true. */
int libc_check() {
  return 0;
}

/* Libc can't be reset. */
void libc_reset_brk() {
}

/* Return NULL for the minimum pointer value.*/
void * libc_heap_lo() {
  return NULL;
}

/* Return NULL.
   This probably isn't portable. */
void * libc_heap_hi() {
  return NULL;
}

/*call default malloc */
void * libc_malloc(size_t size) {
  return malloc(size);
}

/*call default realloc */
void * libc_realloc(void *ptr, size_t size) {
  return realloc(ptr, size);
}

/*call default realloc */
void libc_free(void *ptr) {
  free(ptr);
}
