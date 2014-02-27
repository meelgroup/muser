/*----------------------------------------------------------------------------*\
 * File:        block_scheduler_mt.hh
 *
 * Description: Class declaration and implementation of a block scheduler for 
 *              for multi-threaded environment.
 *
 * Author:      antonb
 * 
 *                                               Copyright (c) 2011, Anton Belov
 \*----------------------------------------------------------------------------*/

#ifndef _BLOCK_SCHEDULER_MT_HH
#define _BLOCK_SCHEDULER_MT_HH 1

#include <tbb/concurrent_queue.h>
#include "basic_group_set.hh"
#include "group_scheduler.hh"
#include "mus_data_mt.hh"

/*----------------------------------------------------------------------------*\
 * Class:  BlockSchedulerMT
 *
 * Purpose: highest-to-lowest scheduler for multithreaded environments: breaks
 *          the GID list into the number of blocks that matches the number of 
 *          workers, and gives out groups from these blocks. If a block for a 
 *          particular worker is empty, it takes them from the next non-empty 
 *          block. This implementation uses concurrent_queue from TBB.
 *
 * Notes:
 *      1. No fasttrack() yet.
 *      2. TODO: perhaps the constructor should take MUSDataMT for cleanness.
 *
\*----------------------------------------------------------------------------*/

class BlockSchedulerMT : public GroupScheduler {

public:

  /** In principle an instance of MUSData and total number of workers is enough */
  BlockSchedulerMT(MUSData& md, unsigned num_workers = 1)
    : GroupScheduler(md, num_workers), _q(num_workers) {
    // populate the queue
    int cutoff = 0;
    int w_idx = -1;
    for (gset_riterator pg = md.gset().grbegin();
         pg != md.gset().grend(); ++pg, --cutoff) {
      if (cutoff == 0) {
        cutoff = 1 + md.gset().gsize() / _num_workers;
        w_idx++;
      }
      if (*pg != 0)
        _q[w_idx].push(*pg);
    }
  }

  /** Returns true and sets the next group id for a given worker ID
   * [0,num_workers) if there's more groups; otherwise false
   */
  virtual bool next_group(GID& next_gid, unsigned worker_id) {
    bool res = _q[worker_id].try_pop(next_gid);
    if (!res) {
      // scan for anything ...
      for (unsigned id = 0; id < _num_workers; id++)
        if (res = _q[id].try_pop(next_gid))
          break;
    }
    return res;
  }

  /** This allows users to re-schedule a group ID check - the invariant is that
   * after this call next_group will give out the gid at some point
   * TODO: there's a potential race here -- figure it out
   */
  virtual void reschedule(GID gid) {
    _q[_num_workers-1].push(gid);
  }

private:

  vector<tbb::concurrent_queue<GID> > _q;  // queues with GIDs

};

#endif // _BLOCK_SCHEDULER_MT_HH

/*----------------------------------------------------------------------------*/
