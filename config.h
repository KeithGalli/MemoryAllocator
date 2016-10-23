#ifndef MM_CONFIG_H
#define MM_CONFIG_H

/*
 * config.h - malloc lab configuration file
 *
 * Copyright (c) 2002, R. Bryant and D. O'Hallaron, All rights reserved.
 * May not be used, modified, or copied without permission.
 */

/*
 * This is the default path where the driver will look for the
 * default tracefiles. You can override it at runtime with the -t flag.
 */
#define TRACEDIR "./traces/"

 /*
  * This constant determines the contributions of space utilization
  * (UTIL_WEIGHT) and throughput (1 - UTIL_WEIGHT) to the performance
  * index.
  */
#define UTIL_WEIGHT (0.50)

#define LIBC_MULTIPLIER (1.10)

#define MAX_BASE_THROUGHPUT (64000e3) /* in kops/sec */

/*
 * Alignment requirement in bytes (8)
 */
#define R_ALIGNMENT 8

/*
 * Maximum heap size in bytes
 */
#define MAX_HEAP (50*(1<<20))  /* 50 MB */

#define MEM_ALLOWANCE (40 * (1 << 10)) /* 40 KB */

/*****************************************************************************
 * Set exactly one of these USE_xxx constants to "1" to select a timing method
 *****************************************************************************/
#define USE_FCYC   0   /* cycle counter w/K-best scheme (x86 & Alpha only) */
#define USE_ITIMER 0   /* interval timer (any Unix box) */
#define USE_GETTOD 1   /* gettimeofday (any Unix box) */

#endif  // MM_CONFIG_H
