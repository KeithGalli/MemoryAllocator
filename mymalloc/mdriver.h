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

#ifndef MM_MDRIVER_H
#define MM_MDRIVER_H

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "./config.h"
#include "./fsecs.h"
#include "./memlib.h"
#include "./allocator_interface.h"

/**********************
 * Constants and macros
 **********************/

/* Misc */
#define MAXLINE     1024 /* max string size */
#define HDRLINES       4 /* number of header lines in a trace file */
#define LINENUM(i) (i+5) /* cnvt trace request nums to linenums (origin 1) */

typedef enum {ALLOC, FREE, REALLOC, WRITE} traceop_type; /* type of request */
/******************************
 * The key compound data types
 *****************************/

/* Characterizes a single trace operation (allocator request) */
typedef struct {
  traceop_type  type; /* type of request */
  int index;                        /* index for free() to use later */
  int size;                         /* byte size of alloc/realloc request */
} traceop_t;

/* Holds the information for one trace file*/
typedef struct {
  int sugg_heapsize;   /* suggested heap size (unused) */
  int num_ids;         /* number of alloc/realloc ids */
  int num_ops;         /* number of distinct requests */
  int weight;          /* weight for this trace (unused) */
  traceop_t *ops;      /* array of requests */
  char **blocks;       /* array of ptrs returned by malloc/realloc... */
  size_t *block_sizes; /* ... and a corresponding array of payload sizes */
} trace_t;

/*********************
 * Function prototypes
 *********************/

void malloc_error(int tracenum, int opnum, char *msg);
void unix_error(char *msg);
void app_error(char *msg);

#endif  // MM_MDRIVER_H
