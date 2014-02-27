//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        rusage_mt.hh
 *
 * Description: additions to RUSAGE namespace to handle multithreaded
 *              executions
 *
 * Author:      antonb
 * 
 * Notes:
 *
 *
 * Revision:    $Id$.
 *
 *                                              Copyright (c) 2011, Anton Belov
\*----------------------------------------------------------------------------*/
//jpms:ec

#ifndef _RUSAGE_MT_HH_
#define _RUSAGE_MT_HH_ 1

#include <time.h>

namespace RUSAGE {
  /** Returns CPU usage for the calling thread (Linux only; 0 on MAC) */
  static inline double read_cpu_time_thread(void);
}

static inline double RUSAGE::read_cpu_time_thread(void)
{
#if (__APPLE__ & __MACH__)
#warning "RUSAGE::read_cpu_time_thread(void) is 0 on MAC OS X"
  return 0;
#endif
#if linux
  struct timespec ts;
  if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0) {
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000;
  }
  return 0;
#endif
}


#endif /* _RUSAGE_MT_HH_ */

/*----------------------------------------------------------------------------*/
