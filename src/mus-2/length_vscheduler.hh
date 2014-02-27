/*----------------------------------------------------------------------------*\
 * File:        length_vscheduler.hh
 *
 * Description: Class declaration and implementation of various schedulers that 
 *              use length of occlists (sum of lengths for groups) to make 
 *              decisions. Note that the schedules are actually dynamic.
 *
 * Author:      antonb
 * 
 *                                               Copyright (c) 2012, Anton Belov
 \*----------------------------------------------------------------------------*/

#ifndef _LENGTH_VSCHEDULER_HH
#define _LENGTH_VSCHEDULER_HH 1

#include <queue>
#include "basic_group_set.hh"
#include "group_scheduler.hh"
#include "mus_data.hh"

/*----------------------------------------------------------------------------*\
 * Class:  LengthVScheduler
 * 
 * Purpose: Scheduler for variables/groups of variables based on lengths of 
 *          occlists.
 *
 * Notes:
 *
\*----------------------------------------------------------------------------*/

class LengthVScheduler : public GroupScheduler {

private:

  // comparator: operator() returns true if g1 < g2; since priority_queue gives
  // the greatest element first, we will return true if the (single) clause from 
  // g2 is shorter than that from g1
  class VGIDLenCompare {
    const BasicGroupSet& _gs;           // reference to the groupset
    unsigned _order;                    // order
  public:
    // order = 1 means longest first, order = 2 means shortest first
    VGIDLenCompare(const BasicGroupSet& gs, unsigned order) 
      : _gs(gs), _order(order) {}
    bool operator()(GID g1, GID g2) {
      const OccsList& occs = _gs.occs_list();
      const VarVector& vs1 = _gs.vgvars(g1);
      unsigned sum1 = 0;
      for (VarVector::const_iterator pv = vs1.begin(); pv != vs1.end(); ++pv)
        sum1 += occs.active_size(*pv) + occs.active_size(-*pv);
      const VarVector& vs2 = _gs.vgvars(g2);
      unsigned sum2 = 0;
      for (VarVector::const_iterator pv = vs2.begin(); pv != vs2.end(); ++pv)
        sum2 += occs.active_size(*pv) + occs.active_size(-*pv);
      return ((_order == 1) ? (sum1 < sum2) : (sum1 > sum2));
    }
  };
  // priority queue using the comparator
  std::priority_queue<GID, std::vector<GID>, VGIDLenCompare> _q;

public:

  /** order = 1 means longest first; order = 2 means shortest first */
  LengthVScheduler(MUSData& md, unsigned order) 
    : GroupScheduler(md), _q(VGIDLenCompare(md.gset(), order)) {
    // populate the queue
    for (vgset_iterator pg = md.gset().vgbegin(); pg != md.gset().vgend(); ++pg) {
      if (*pg != 0)
        _q.push(*pg);
    }
  }

  /** Returns true and sets the next group id for a given worker ID
   * [0,num_workers) if there's more groups; otherwise false
   */
  virtual bool next_group(GID& next_gid, unsigned worker_id = 0) {
    if (_q.empty())
      return false;
    next_gid = _q.top();
    _q.pop();
    return true;
  }

  /** This allows users to re-schedule a group ID check - the invariant is that
   * after this call next_group will give out the gid at some point;
   */
  virtual void reschedule(GID gid) {
    _q.push(gid);
  }

  /** This allows to push some gids to the front (if this makes sense - it doesn't
   * for this scheduler; if this functionality is really needed, make another 
   * regular queue for fasttracked items)
   */
  virtual void fasttrack(GID gid) {
    _q.push(gid);
  }

};

#endif // _LENGTH_VSCHEDULER_HH

/*----------------------------------------------------------------------------*/
