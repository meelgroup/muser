/*----------------------------------------------------------------------------*\
 * File:        linear_scheduler.hh
 *
 * Description: Class declaration and implementation of a simple linear (largest
 *              to smallest gid) group scheduler. Not MT-safe.
 *
 * Author:      antonb
 * 
 *                                               Copyright (c) 2011, Anton Belov
 \*----------------------------------------------------------------------------*/

#ifndef _LINEAR_SCHEDULER_HH
#define _LINEAR_SCHEDULER_HH 1

#include <deque>
#include "basic_group_set.hh"
#include "group_scheduler.hh"
#include "mus_data.hh"

/*----------------------------------------------------------------------------*\
 * Class:  LinearScheduler
 *
 * Purpose: Simple linear highest-to-lowest scheduler for single-threaded 
 *          environment.
 *
 * Notes:
 *
 *      1. IMPORTANT: not MT-safe (use LinearSchedulerMT if needed)
 *
\*----------------------------------------------------------------------------*/

class LinearScheduler : public GroupScheduler {

public:

  /** In principle an instance of MUSData and total number of workers is enough */
  LinearScheduler(MUSData& md, bool reverse = false) 
    : GroupScheduler(md) {
    // populate the queue
    for (gset_iterator pg = md.gset().gbegin(); pg != md.gset().gend(); ++pg)
      if (*pg != 0) {
        (reverse) ? _q.push_back(*pg) : _q.push_front(*pg);
      }
  }

  /** Returns true and sets the next group id for a given worker ID
   * [0,num_workers) if there's more groups; otherwise false
   */
  virtual bool next_group(GID& next_gid, unsigned worker_id = 0) {
    if (_q.empty())
      return false;
    next_gid = _q.front();
    _q.pop_front();
    return true;
  }

  /** This allows users to re-schedule a group ID check - the invariant is that
   * after this call next_group will give out the gid at some point
   * TODO: there's a potential race here -- figure it out
   */
  virtual void reschedule(GID gid) {
    _q.push_back(gid);
  }

  /** This allows to push some gids to the front (if this makes sense)
   */
  virtual void fasttrack(GID gid) {
    _q.push_front(gid);
  }

private:

  std::deque<GID> _q;       // queue with GIDs - give one at a time

};

#endif // _LINEAR_SCHEDULER_HH

/*----------------------------------------------------------------------------*/
