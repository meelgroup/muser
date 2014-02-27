/*----------------------------------------------------------------------------*\
 * File:        worker.hh
 *
 * Description: Abstract base class definition of workers.
 *
 * Author:      antonb
 * 
 * Notes:
 *
 *                                              Copyright (c) 2011, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _WORKER_HH
#define _WORKER_HH

#include "work_item.hh"

/*----------------------------------------------------------------------------*\
 * Class:  Worker
 *
 * Purpose: Abstract base for all workers
 *
 * Notes: 
 *      1. Workers are (typically) not MT-safe, however they should be designed 
 * for use in MT environment, where by multiple instances of workers run side
 * by side on multiple threads
 * 
 *
\*----------------------------------------------------------------------------*/

class Worker {

public:

  // lifecycle

  Worker(unsigned id = 0) : _id(id) {}

  virtual ~Worker(void) {}

  /** To be implemented by subclasses for specific work items -- return true if
   * ihe item is completed, false otherwise.
   */
  virtual bool process(WorkItem& wi) { return false; }

  /* Returns the ID of this worker */
  unsigned id(void) const { return _id; }

protected:

  unsigned _id;

};

#endif /* _WORKER_HH */

/*----------------------------------------------------------------------------*/
