/*
 * mdriver.c - CS:APP Malloc Lab Driver
 *
 * Uses a collection of trace files to tests a malloc/free/realloc
 * implementation in mm.c.
 *
 * There should be no reason to edit this code.
 *
 * Copyright (c) 2002, R. Bryant and D. O'Hallaron, All rights reserved.
 * May not be used, modified, or copied without permission.
 */

#include "./mdriver.h"
#include "./validator.h"

#ifdef GET_RUNNINGTIME
#include "./fasttime.h"
#endif
/******************************
 * Private compound data types
 *****************************/

/* Summarizes the important stats for some malloc function on some trace */
typedef struct {
  /* defined for both libc malloc and student malloc package (mm.c) */
  double ops;      /* number of ops (malloc/free/realloc) in the trace */
  int valid;       /* was the trace processed correctly by the allocator? */
  int checked;     /* was the heap valid after every allocation? */
  double secs;     /* number of secs needed to run the trace */

  /* defined only for the student malloc package */
  double util;     /* space utilization for this trace (always 0 for libc) */

  /* Note: secs and util are only defined if valid is true */
} stats_t;

/********************
 * Global variables
 *******************/
int verbose = 0;        /* global flag for verbose output */
static int errors = 0;  /* number of errs found when running student malloc */
char msg[MAXLINE];      /* for whenever we need to compose an error message */

/* Directory where default tracefiles are found */
static char tracedir[MAXLINE] = TRACEDIR;

static const char xor_constant = 0x7B;

/*********************
 * Function prototypes
 *********************/

/* These functions read, allocate, and free storage for traces */
static trace_t *read_trace(char *tracedir, char *filename);
static void free_trace(trace_t *trace);

/* Routines for evaluating correctnes, space utilization, and speed
   of the student's malloc package in mm.c */
static double eval_mm_util(const malloc_impl_t *impl, trace_t *trace, int tracenum);
static void eval_mm_speed(const malloc_impl_t *impl, trace_t *trace);
static void eval_my_speed(trace_t *trace) {
  eval_mm_speed(&my_impl, trace);
}
static void eval_libc_speed(trace_t *trace) {
  eval_mm_speed(&libc_impl, trace);
}
static int eval_mm_check(const malloc_impl_t *impl, trace_t *trace, int tracenum);

/* Various helper routines */
static void printresults(int n, char **tracefiles, stats_t *stats);
static void usage(void);

/**************
 * Main routine
 **************/
int main(int argc, char **argv) {
#ifdef GET_RUNNINGTIME
  fasttime_t begin = gettime();
#endif
  int i;
  char c;
  char **tracefiles = NULL;  /* null-terminated array of trace file names */
  int num_tracefiles = 0;    /* the number of traces in that array */
  trace_t *trace = NULL;     /* stores a single trace file in memory */
  stats_t *libc_stats = NULL;/* libc stats for each trace */
  stats_t *bad_stats = NULL; /* bad malloc stats for each trace */
  stats_t *mm_stats = NULL;  /* mm (i.e. student) stats for each trace */

  int run_bad = 0;     /* If set, run bad malloc (set by -b) */
  int check_heap = 0;  /* If set, run the student heap checker (set by -c) */
  int autograder = 0;  /* If set, emit summary info for autograder (-g) */

  /* temporaries used to compute the performance index */
  double total_throughput, total_util, average_util, average_throughput, p1, p2, perfindex;
  int numcorrect;

  /*
   * Read and interpret the command line arguments
   */
  while ((c = getopt(argc, argv, "f:t:hvVgcb")) != EOF) {
    switch (c) {
      case 'g': /* Generate summary info for the autograder */
        autograder = 1;
        break;
      case 'f': /* Use one specific trace file only (relative to curr dir) */
        num_tracefiles = 1;
        if ((tracefiles = (char **) realloc(tracefiles, 2*sizeof(char *))) == NULL)
          unix_error("ERROR: realloc failed in main");
        strcpy(tracedir, "./");
        tracefiles[0] = strdup(optarg);
        tracefiles[1] = NULL;
        break;
      case 't': /* Directory where the traces are located */
        if (num_tracefiles == 1) /* ignore if -f already encountered */
          break;
        strcpy(tracedir, optarg);
        if (tracedir[strlen(tracedir)-1] != '/')
          strcat(tracedir, "/"); /* path always ends with "/" */
        break;
      case 'b': /* Run bad malloc to check the verifier. */
        run_bad = 1;
        break;
      case 'c':
        check_heap = 1;
        break;
      case 'v': /* Print per-trace performance breakdown */
        verbose = 1;
        break;
      case 'V': /* Be more verbose than -v */
        verbose = 2;
        break;
      case 'h': /* Print this message */
        usage();
        exit(0);
      default:
        usage();
        exit(1);
    }
  }

  /*
   * If no -f command line arg, then use the entire set of tracefiles
   * defined in default_traces[]
   */
  if (tracefiles == NULL) {
    printf("Using tracefiles in %s\n", tracedir);

    DIR * dirp = opendir(tracedir);
    if (!dirp) {
      fprintf(stderr, "Cannot open directory '%s': %s\n",
              tracedir, strerror(errno));
      exit(EXIT_FAILURE);
    }
    while (1) {
      struct dirent *entry = readdir(dirp);
      if (!entry) {
        break;
      }
      const char *filename = entry->d_name;
      if (filename && filename[0] != '.') {  /* skip . and .. */
        num_tracefiles++;
        if ((tracefiles = (char **) realloc(tracefiles,
            (num_tracefiles + 1) * sizeof(char *))) == NULL)
          unix_error("ERROR: realloc failed in main");
        tracefiles[num_tracefiles - 1] = strdup(filename);
        tracefiles[num_tracefiles] = NULL;
      }
    }
    if (closedir(dirp)) {
      fprintf(stderr, "Could not close '%s': %s\n",
              tracedir, strerror(errno));
      exit(EXIT_FAILURE);
    }
  }

  /* Initialize the timing package */
  init_fsecs();

  /*
   * Always run and evaluate the libc malloc package
   */
  if (verbose > 1) {
    printf("\nTesting libc malloc\n");
  }
  /* Allocate libc stats array, with one stats_t struct per tracefile */
  libc_stats = (stats_t *)calloc(num_tracefiles, sizeof(stats_t));
  if (libc_stats == NULL) {
    unix_error("libc_stats calloc in main failed");
  }

  /* Evaluate the libc malloc package using the K-best scheme */
  for (i = 0; i < num_tracefiles; i++) {
    trace = read_trace(tracedir, tracefiles[i]);
    libc_stats[i].ops = trace->num_ops;
    if (verbose > 1)
      printf("Checking libc malloc for correctness, ");
    libc_stats[i].valid = 1;
    if (check_heap) {
      libc_stats[i].checked = eval_mm_check(&libc_impl, trace, i);
    }
    if (libc_stats[i].valid) {
      if (verbose > 1)
        printf("and performance.\n");
      libc_stats[i].secs = fsecs((void (*)(void *))eval_libc_speed, trace);
    }
    free_trace(trace);
  }

  /* Display the libc results in a compact table */
  if (verbose) {
    printf("\nResults for libc malloc:\n");
    printresults(num_tracefiles, tracefiles, libc_stats);
  }

  /* Initialize the simulated memory system in memlib.c */
  mem_init();

  /*
   * Optionally run and evaluate the bad malloc package
   */
  if (run_bad) {
    if (verbose > 1) {
      printf("\nTesting bad malloc\n");
    }

    /* Allocate bad stats array, with one stats_t struct per tracefile */
    bad_stats = (stats_t *)calloc(num_tracefiles, sizeof(stats_t));
    if (bad_stats == NULL) {
      unix_error("bad_stats calloc in main failed");
    }

    /* Evaluate the bad malloc package using the K-best scheme */
    for (i = 0; i < num_tracefiles; i++) {
      trace = read_trace(tracedir, tracefiles[i]);
      bad_stats[i].ops = trace->num_ops;
      printf("Checking bad malloc for correctness.\n");
      bad_stats[i].valid = eval_mm_valid(&bad_impl, trace, i);
      if (check_heap) {
        bad_stats[i].checked = eval_mm_check(&bad_impl, trace, i);
      }
      free_trace(trace);
    }

    /* Display the bad results in a compact table */
    if (verbose) {
      printf("\nResults for bad malloc:\n");
      printresults(num_tracefiles, tracefiles, bad_stats);
    }
  }

  /*
   * Always run and evaluate the student's mm package
   */
  if (verbose > 1) {
    printf("\nTesting mm malloc\n");
  }

  /* Allocate the mm stats array, with one stats_t struct per tracefile */
  mm_stats = (stats_t *)calloc(num_tracefiles, sizeof(stats_t));
  if (mm_stats == NULL) {
    unix_error("mm_stats calloc in main failed");
  }

  /* Evaluate student's mm malloc package using the K-best scheme */
  for (i = 0; i < num_tracefiles; i++) {
    trace = read_trace(tracedir, tracefiles[i]);
    mm_stats[i].ops = trace->num_ops;
    if (verbose > 1) {
      printf("Checking mm_malloc for correctness, ");
    }
    mm_stats[i].valid = eval_mm_valid(&my_impl, trace, i);
    if (check_heap) {
      mm_stats[i].checked = eval_mm_check(&my_impl, trace, i);
    }
    if (mm_stats[i].valid) {
      if (verbose > 1) {
        printf("efficiency, ");
      }
      mm_stats[i].util = eval_mm_util(&my_impl, trace, i);
      if (verbose > 1) {
        printf("and performance.\n");
      }
      mm_stats[i].secs = fsecs((void (*)(void *))eval_my_speed, trace);
    }
    free_trace(trace);
  }

  /* Free the simulated heap block. */
  mem_deinit();

  /* Display the mm results in a compact table */
  if (verbose) {
    printf("\nResults for mm malloc:\n");
    printresults(num_tracefiles, tracefiles, mm_stats);
    printf("\n");
  }

  /*
   * Accumulate the aggregate statistics for the student's mm package
   */
  total_throughput = 0;
  total_util = 0;
  numcorrect = 0;
  if (verbose) {
    printf("(throughput)%18s%8s%8s%8s%7s%7s\n",
           "filename", "libc", "base", "my", "", "(util)");
  }
  for (i = 0; i < num_tracefiles; i++) {
    if (mm_stats[i].valid) {
      numcorrect++;

      total_util += mm_stats[i].util;

      double my_throughput = mm_stats[i].ops / mm_stats[i].secs;
      double libc_throughput = libc_stats[i].ops / libc_stats[i].secs;
      double base_throughput = LIBC_MULTIPLIER * libc_throughput;
      if (base_throughput > MAX_BASE_THROUGHPUT)
        base_throughput = MAX_BASE_THROUGHPUT;
      double ratio = my_throughput / base_throughput;
      if (ratio > 1.0)
        ratio = 1.0;
      total_throughput += ratio;

      if (verbose) {
        printf("%30s%8.0f%8.0f%8.0f%6.0f%%%6.0f%%\n",
               tracefiles[i], libc_throughput/1000, base_throughput/1000,
               my_throughput/1000, ratio*100, mm_stats[i].util*100);
      }
    }
  }
  average_throughput = total_throughput/num_tracefiles;
  average_util = total_util/num_tracefiles;

  /*
   * Compute and print the performance index
   */
  p1 = 100.0 * UTIL_WEIGHT * average_util;
  p2 = 100.0 * (1.0 - UTIL_WEIGHT) * average_throughput;
  perfindex = p1 + p2;

  printf("# %f (util)  +  %f (tput)  =  %f\n", p1, p2, perfindex);

  if (autograder) {
    printf("correct:%d\n", numcorrect);
    printf("perfidx:%f\n", perfindex);
  }

  if (errors != 0) {
    printf("Terminated with %d errors\n", errors);
  }

  /* Keep valgrind happy, free the arrays. */
  free(libc_stats);
  free(bad_stats);
  free(mm_stats);

  for (i = 0; i < num_tracefiles; i++) {
    free(tracefiles[i]);
  }
  free(tracefiles);

#ifdef GET_RUNNINGTIME
  fasttime_t end = gettime();
  printf("runtime:%f\n", tdiff(begin, end));
#endif

  exit(0);
}

/**********************************************
 * The following routines manipulate tracefiles
 *********************************************/

/*
 * read_trace - read a trace file and store it in memory
 */
static trace_t *read_trace(char *tracedir, char *filename) {
  FILE *tracefile;
  trace_t *trace;
  char type[MAXLINE];
  char path[MAXLINE];
  unsigned index, size;
  unsigned max_index = 0;
  unsigned op_index;

  if (verbose > 1) {
    printf("Reading tracefile: %s\n", filename);
  }

  /* Allocate the trace record */
  if ((trace = (trace_t *) malloc(sizeof(trace_t))) == NULL) {
    unix_error("malloc 1 failed in read_trance");
  }

  /* Read the trace file header */
  strcpy(path, tracedir);
  strcat(path, filename);
  if ((tracefile = fopen(path, "r")) == NULL) {
    sprintf(msg, "Could not open %s in read_trace", path);
    unix_error(msg);
  }
  fscanf(tracefile, "%d", &(trace->sugg_heapsize)); /* not used */
  fscanf(tracefile, "%d", &(trace->num_ids));
  fscanf(tracefile, "%d", &(trace->num_ops));
  fscanf(tracefile, "%d", &(trace->weight));        /* not used */

  /* We'll store each request line in the trace in this array */
  if ((trace->ops =
       (traceop_t *)malloc(trace->num_ops * sizeof(traceop_t))) == NULL) {
    unix_error("malloc 2 failed in read_trace");
  }

  /* We'll keep an array of pointers to the allocated blocks here... */
  if ((trace->blocks =
       (char **)malloc(trace->num_ids * sizeof(char *))) == NULL) {
    unix_error("malloc 3 failed in read_trace");
  }

  /* ... along with the corresponding byte sizes of each block */
  if ((trace->block_sizes =
       (size_t *)malloc(trace->num_ids * sizeof(size_t))) == NULL) {
    unix_error("malloc 4 failed in read_trace");
  }

  /* read every request line in the trace file */
  index = 0;
  op_index = 0;
  while (fscanf(tracefile, "%s", type) != EOF) {
    switch (type[0]) {
      case 'a':
        fscanf(tracefile, "%u %u", &index, &size);
        trace->ops[op_index].type = ALLOC;
        trace->ops[op_index].index = index;
        trace->ops[op_index].size = size;
        max_index = (index > max_index) ? index : max_index;
        break;
      case 'r':
        fscanf(tracefile, "%u %u", &index, &size);
        trace->ops[op_index].type = REALLOC;
        trace->ops[op_index].index = index;
        trace->ops[op_index].size = size;
        max_index = (index > max_index) ? index : max_index;
        break;
      case 'f':
        fscanf(tracefile, "%ud", &index);
        trace->ops[op_index].type = FREE;
        trace->ops[op_index].index = index;
        break;
      case 'w':
        fscanf(tracefile, "%u %u", &index, &size);
        trace->ops[op_index].type = WRITE;
        trace->ops[op_index].index = index;
        trace->ops[op_index].size = size;
        break;
      default:
        printf("Bogus type character (%c) in tracefile %s\n",
               type[0], path);
        exit(1);
    }
    op_index++;
  }
  fclose(tracefile);
  assert((int) max_index == trace->num_ids - 1);
  assert(trace->num_ops == (int) op_index);

  return trace;
}

/*
 * free_trace - Free the trace record and the three arrays it points
 *              to, all of which were allocated in read_trace().
 */
void free_trace(trace_t *trace) {
  free(trace->ops);         /* free the three arrays... */
  free(trace->blocks);
  free(trace->block_sizes);
  free(trace);              /* and the trace record itself... */
}

/**********************************************************************
 * The following functions evaluate the space utilization and
 * throughput of the libc and mm malloc packages.
 **********************************************************************/

/*
 * eval_mm_util - Evaluate the space utilization of the student's package
 *   The idea is to remember the high water mark "hwm" of the heap for
 *   an optimal allocator, i.e., no gaps and no internal fragmentation.
 *   Utilization is the ratio hwm/heapsize, where heapsize is the
 *   size of the heap in bytes after running the student's malloc
 *   package on the trace.
 *
 */
static double eval_mm_util(const malloc_impl_t *impl, trace_t *trace, int tracenum) {
  int i;
  int index;
  int size, newsize, oldsize;
  int max_total_size = 0;
  int total_size = 0;
  size_t heap_size = 0;
  char *p;
  char *newp, *oldp;

  /* initialize the heap and the mm malloc package */
  mem_reset_brk();
  if (impl->init() < 0) {
    app_error("init failed in eval_mm_util");
  }

  for (i = 0; i < trace->num_ops; i++) {
    switch (trace->ops[i].type) {
      case ALLOC: /* alloc */
        index = trace->ops[i].index;
        size = trace->ops[i].size;

        if ((p = (char *) impl->malloc(size)) == NULL) {
          app_error("malloc failed in eval_mm_util");
        }

        /* Remember region and size */
        trace->blocks[index] = p;
        trace->block_sizes[index] = size;

        /* Keep track of current total size
         * of all allocated blocks */
        total_size += size;

        /* Update statistics */
        max_total_size = (total_size > max_total_size) ?
            total_size : max_total_size;
        break;

      case REALLOC: /* realloc */
        index = trace->ops[i].index;
        newsize = trace->ops[i].size;
        oldsize = trace->block_sizes[index];

        oldp = trace->blocks[index];
        if ((newp = (char *) impl->realloc(oldp, newsize)) == NULL)
          app_error("realloc failed in eval_mm_util");

        /* Remember region and size */
        trace->blocks[index] = newp;
        trace->block_sizes[index] = newsize;

        /* Keep track of current total size
         * of all allocated blocks */
        total_size += (newsize - oldsize);

        /* Update statistics */
        max_total_size = (total_size > max_total_size) ?
            total_size : max_total_size;
        break;

      case FREE: /* free */
        index = trace->ops[i].index;
        size = trace->block_sizes[index];
        p = trace->blocks[index];

        impl->free(p);

        /* Keep track of current total size
         * of all allocated blocks */
        total_size -= size;

        break;

      case WRITE: /* write */
        break;

      default:
        app_error("Nonexistent request type in eval_mm_util");
    }
  }
  max_total_size = (max_total_size > MEM_ALLOWANCE) ?
    max_total_size : MEM_ALLOWANCE;
  heap_size = mem_heapsize();
  heap_size = (heap_size > MEM_ALLOWANCE) ?
    heap_size : MEM_ALLOWANCE;
  return ((double)max_total_size / (double)heap_size);
}

static void mem_op(volatile char *raddr, volatile char *waddr) {
  *waddr = *raddr ^ xor_constant;
}

/*
 * eval_mm_speed - This is the function that is used by fcyc()
 *    to measure the running time of the mm malloc package.
 */
static void eval_mm_speed(const malloc_impl_t *impl, trace_t *trace) {
  int i, index, size, newsize;
  char *p, *newp, *oldp, *block;

  /* Reset the heap and initialize the mm package */
  mem_reset_brk();
  if (impl->init() < 0) {
    app_error("init failed in eval_mm_speed");
  }

  /* Interpret each trace request */
  for (i = 0; i < trace->num_ops; i++) {
    switch (trace->ops[i].type) {
      case ALLOC: /* malloc */
        index = trace->ops[i].index;
        size = trace->ops[i].size;
        if ((p = (char *) impl->malloc(size)) == NULL)
          app_error("malloc error in eval_mm_speed");
        trace->blocks[index] = p;
        break;

      case REALLOC: /* realloc */
        index = trace->ops[i].index;
        newsize = trace->ops[i].size;
        oldp = trace->blocks[index];
        if ((newp = (char *) impl->realloc(oldp, newsize)) == NULL)
          app_error("realloc error in eval_mm_speed");
        trace->blocks[index] = newp;
        break;

      case FREE: /* free */
        index = trace->ops[i].index;
        block = trace->blocks[index];
        impl->free(block);
        break;

      case WRITE: /* write */
        index = trace->ops[i].index;
        size = trace->ops[i].size;
        p = trace->blocks[index];
        if (size > 1) {
          /* read bytes, do some computation, and write */
          for (int offset = 1; offset < size; offset++) {
            mem_op(p + offset - 1, p + offset);
          }
        }
        break;

      default:
        app_error("Nonexistent request type in eval_mm_speed");
    }
  }
}

/*
 * eval_mm_check - This function is used to check the heap of the student's
 *    implementation.  Returns 0 on check failure, and 1 on pass.
 */
static int eval_mm_check(const malloc_impl_t *impl, trace_t *trace, int tracenum) {
  int i, index, size, newsize;
  char *p, *newp, *oldp, *block;

  /* Reset the heap and initialize the mm package */
  mem_reset_brk();
  if (impl->init() < 0) {
    malloc_error(tracenum, 0, "impl init failed.");
  }
  /* Interpret each trace request */
  for (i = 0; i < trace->num_ops; i++) {
    switch (trace->ops[i].type) {
      case ALLOC: /* malloc */
        index = trace->ops[i].index;
        size = trace->ops[i].size;
        if ((p = (char *) impl->malloc(size)) == NULL) {
          malloc_error(tracenum, i, "impl malloc failed.");
          return 0;
        }
        trace->blocks[index] = p;
        break;

      case REALLOC: /* realloc */
        index = trace->ops[i].index;
        newsize = trace->ops[i].size;
        oldp = trace->blocks[index];
        if ((newp = (char *) impl->realloc(oldp, newsize)) == NULL) {
          malloc_error(tracenum, i, "impl realloc failed.");
          return 0;
        }
        trace->blocks[index] = newp;
        break;

      case FREE: /* free */
        index = trace->ops[i].index;
        block = trace->blocks[index];
        impl->free(block);
        break;

      case WRITE: /* write */
        break;

      default:
        app_error("Nonexistent request type in eval_mm_check");
    }

    if (impl->check() < 0) {
      malloc_error(tracenum, i, "impl check failed.");
      return 0;
    }
  }

  return 1;
}

/*************************************
 * Some miscellaneous helper routines
 ************************************/


/*
 * printresults - prints a performance summary for some malloc package
 */
static void printresults(int n, char **tracefiles, stats_t *stats) {
  int i;
  double total_ops = 0, total_secs = 0, total_throughput = 0, total_util = 0;

  /* Print the individual results for each trace */
  printf("%5s%27s%10s%10s%6s%8s%10s%9s\n",
         "trace", "filename", " valid", "checked", "util", "ops", "secs", "Kops/sec");
  for (i = 0; i < n; i++) {
    if (stats[i].valid) {
      double throughput = (stats[i].ops/stats[i].secs)/1e3;
      printf("%2d%30s%10s%10s%5.0f%%%8.0f%10.6f %8.0f\n",
             i,
             tracefiles[i],
             "yes",
             (stats[i].checked ? "yes" : "no"),
             stats[i].util*100.0,
             stats[i].ops,
             stats[i].secs,
             throughput);
      total_ops += stats[i].ops;
      total_secs += stats[i].secs;
      total_throughput += throughput;
      total_util += stats[i].util;
    } else {
      printf("%2d%30s%10s%10s%6s%8s%10s%8s\n",
             i,
             tracefiles[i],
             "no",
             (stats[i].checked ? "yes" : "no"),
             "-",
             "-",
             "-",
             "-");
    }
  }

  /* Print the aggregate results for the set of traces */
  if (errors == 0) {
    printf("%12s%40s%5.0f%%%8.0f%10.6f %8.0f\n",
           "Average     ",
           "",
           (total_util/n)*100.0,
           total_ops,
           total_secs,
           total_throughput/n);
  } else {
    printf("%12s%40s%6s%8s%10s%8s\n",
           "Average     ",
           "",
           "-",
           "-",
           "-",
           "-");
  }
}

/*
 * app_error - Report an arbitrary application error
 */
void app_error(char *msg) {
  printf("%s\n", msg);
  exit(1);
}

/*
 * unix_error - Report a Unix-style error
 */
void unix_error(char *msg) {
  printf("%s: %s\n", msg, strerror(errno));
  exit(1);
}

/*
 * malloc_error - Report an error returned by the mm_malloc package
 */
void malloc_error(int tracenum, int opnum, char *msg) {
  errors++;
  printf("ERROR [trace %d, line %d]: %s\n", tracenum, LINENUM(opnum), msg);
}

/*
 * usage - Explain the command line arguments
 */
static void usage(void) {
  fprintf(stderr, "Usage: mdriver [-hvVgc] [-f <file>] [-t <dir>]\n");
  fprintf(stderr, "Options\n");
  fprintf(stderr, "\t-f <file>  Use <file> as the trace file.\n");
  fprintf(stderr, "\t-t <dir>   Directory to find default traces.\n");
  fprintf(stderr, "\t-g         Generate summary info for autograder.\n");
  fprintf(stderr, "\t-v         Print per-trace performance breakdowns.\n");
  fprintf(stderr, "\t-V         Print additional debug info.\n");
  fprintf(stderr, "\t-c         Check the heap after every operation.\n");
  fprintf(stderr, "\t-h         Print this message.\n");
}
