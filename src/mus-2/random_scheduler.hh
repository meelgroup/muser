/*----------------------------------------------------------------------------*\
 * File:        random_scheduler.hh
 *
 * Description: Class declaration and implementation of a random scheduler.
 *
 * Author:      antonb
 * 
 *                                               Copyright (c) 2012, Anton Belov
 \*----------------------------------------------------------------------------*/

#ifndef _RANDOM_SCHEDULER_HH
#define _RANDOM_SCHEDULER_HH 1

#include <ctime>
#include <deque>
#include <random>
#include <vector>
#include "basic_group_set.hh"
#include "group_scheduler.hh"
#include "mus_data.hh"

/*----------------------------------------------------------------------------*\
 * Class:  RandomScheduler
 *
 * Purpose: Simple random-order scheduler.
 *
 * Notes:
 *
 *      1. IMPORTANT: not MT-safe
 *
\*----------------------------------------------------------------------------*/

class RandomScheduler : public GroupScheduler {

public:

  RandomScheduler(MUSData& md) 
  : GroupScheduler(md) {
    // populate the queue
    std::vector<GID> v;
    for (auto pg = md.gset().gbegin(); pg != md.gset().gend(); ++pg)
      if (*pg != 0)
        v.push_back(*pg);
    std::minstd_rand g(std::time(0)); // seed it 
    std::shuffle(v.begin(), v.end(), g);
    for (GID gid : v) 
      if (gid) _q.push_back(gid);
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

#endif // _RANDOM_SCHEDULER_HH

/*----------------------------------------------------------------------------*/
