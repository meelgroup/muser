/*----------------------------------------------------------------------------*\
 * File:        linear_vscheduler.hh
 *
 * Description: Class declaration and implementation of a simple linear (smallest
 *              to largest vgid) variable group scheduler. Not MT-safe.
 *
 * Author:      antonb
 * 
 *                                               Copyright (c) 2012, Anton Belov
 \*----------------------------------------------------------------------------*/

#ifndef _LINEAR_VSCHEDULER_HH
#define _LINEAR_VSCHEDULER_HH 1

#include <deque>
#include "basic_group_set.hh"
#include "group_scheduler.hh"
#include "mus_data.hh"

/*----------------------------------------------------------------------------*\
 * Class:  LinearVScheduler
 *
 * Purpose: Simple linear lowest-to-highest scheduler for single-threaded 
 *          environment.
 *
 * Notes:
 *
\*----------------------------------------------------------------------------*/

class LinearVScheduler : public GroupScheduler {

public:

  /** In principle an instance of MUSData and total number of workers is enough.
   */
  LinearVScheduler(MUSData& md, bool reverse = false)         
    : GroupScheduler(md) {
    // populate the queue
    for (vgset_iterator pvg = md.gset().vgbegin(); pvg != md.gset().vgend(); ++pvg)
      if (*pvg != 0) {
        (reverse) ? _q.push_front(*pvg) : _q.push_back(*pvg);
      }
  }

  /** Returns true and sets the next group id for a given worker ID
   * [0,num_workers) if there's more groups; otherwise false
   */
  virtual bool next_group(GID& next_vgid, unsigned worker_id = 0) {
    if (_q.empty())
      return false;
    next_vgid = _q.front();
    _q.pop_front();
    return true;
  }

  /** This allows users to re-schedule a group ID check - the invariant is that
   * after this call next_group will give out the gid at some point
   * TODO: there's a potential race here -- figure it out
   */
  virtual void reschedule(GID vgid) {
    _q.push_back(vgid);
  }

  /** This allows to push some gids to the front (if this makes sense)
   */
  virtual void fasttrack(GID vgid) {
    _q.push_front(vgid);
  }

private:

  std::deque<GID> _q;       // queue with GIDs - give one at a time

};

#endif // _LINEAR_VSCHEDULER_HH

/*----------------------------------------------------------------------------*/
