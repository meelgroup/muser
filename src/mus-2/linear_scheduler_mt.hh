/*----------------------------------------------------------------------------*\
 * File:        linear_scheduler_mt.hh
 *
 * Description: Class declaration and implementation of a linear (largest to 
 *              smallest gid) group scheduler for multi-threaded environment.
 *
 * Author:      antonb
 * 
 *                                               Copyright (c) 2011, Anton Belov
 \*----------------------------------------------------------------------------*/

#ifndef _LINEAR_SCHEDULER_MT_HH
#define _LINEAR_SCHEDULER_MT_HH 1

#include <tbb/concurrent_queue.h>
#include "basic_group_set.hh"
#include "group_scheduler.hh"
#include "mus_data_mt.hh"

/*----------------------------------------------------------------------------*\
 * Class:  LinearSchedulerMT
 *
 * Purpose: Simple linear highest-to-lowest scheduler for multithreaded 
 *          environments. This implementation uses concurrent_queue from TBB.
 *
 * Notes:
 *      1. No fasttrack() yet.
 *      2. TODO: perhaps the constructor should take MUSDataMT for cleanness.
 *
\*----------------------------------------------------------------------------*/

class LinearSchedulerMT : public GroupScheduler {

public:

  /** In principle an instance of MUSData and total number of workers is enough */
  LinearSchedulerMT(MUSData& md, unsigned num_workers = 1)
    : GroupScheduler(md, num_workers) {
    // populate the queue (TODO: fix the ordering)
    for (gset_iterator pg = md.gset().gbegin(); pg != md.gset().gend(); ++pg)
      if (*pg != 0)
        _q.push(*pg);
  }

  /** Returns true and sets the next group id for a given worker ID
   * [0,num_workers) if there's more groups; otherwise false
   */
  virtual bool next_group(GID& next_gid, unsigned worker_id) {
    return _q.try_pop(next_gid);
  }

  /** This allows users to re-schedule a group ID check - the invariant is that
   * after this call next_group will give out the gid at some point
   * TODO: there's a potential race here -- figure it out
   */
  virtual void reschedule(GID gid) {
    _q.push(gid);
  }

private:

  tbb::concurrent_queue<GID> _q;       // queue with GIDs - give one at a time

};

#endif // _LINEAR_SCHEDULER_MT_HH

/*----------------------------------------------------------------------------*/
