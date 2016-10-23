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

/****************************
 * High-level timing wrappers
 ****************************/
#include <stdio.h>
#include "./fsecs.h"
#include "./fcyc.h"
#include "./clock.h"
#include "./ftimer.h"
#include "./config.h"

static double Mhz;  /* estimated CPU clock frequency */

extern int verbose; /* -v option in mdriver.c */

/*
 * init_fsecs - initialize the timing package
 */
void init_fsecs(void) {
  Mhz = 0; /* keep gcc -Wall happy */

#if USE_FCYC
  if (verbose)
    printf("Measuring performance with a cycle counter.\n");

  /* set key parameters for the fcyc package */
  set_fcyc_maxsamples(20);
  set_fcyc_clear_cache(1);
  set_fcyc_compensate(1);
  set_fcyc_epsilon(0.01);
  set_fcyc_k(3);
  Mhz = mhz(verbose > 0);
#elif USE_ITIMER
  if (verbose)
    printf("Measuring performance with the interval timer.\n");
#elif USE_GETTOD
  if (verbose)
    printf("Measuring performance with gettimeofday().\n");
#endif
}

/*
 * fsecs - Return the running time of a function f (in seconds)
 */
double fsecs(fsecs_test_funct f, void *argp) {
#if USE_FCYC
  double cycles = fcyc(f, argp);
  return cycles/(Mhz*1e6);
#elif USE_ITIMER
  return ftimer_itimer(f, argp, 10);
#elif USE_GETTOD
  return ftimer_gettod(f, argp, 10);
#endif
}
