/*----------------------------------------------------------------------------*\
 * File:        length_scheduler.hh
 *
 * Description: Class declaration and implementation of various schedulers that 
 *              use length of clauses (sum of lengths for groups) to make 
 *              decisions. Note that the schedules are actually dynamic.
 *
 * Author:      antonb
 * 
 *                                               Copyright (c) 2012, Anton Belov
 \*----------------------------------------------------------------------------*/

#ifndef _LENGTH_SCHEDULER_HH
#define _LENGTH_SCHEDULER_HH 1

#include <queue>
#include "basic_group_set.hh"
#include "group_scheduler.hh"
#include "mus_data.hh"

/*----------------------------------------------------------------------------*\
 * Class:  LengthScheduler
 * 
 * Purpose: Scheduler for clauses/groups of clauses based on lengths.
 *
 * Notes:
 *
 *
\*----------------------------------------------------------------------------*/

class LengthScheduler : public GroupScheduler {

private:

  // comparator: operator() returns true if g1 < g2; since priority_queue gives
  // the greatest element first, we will return true if the (single) clause from 
  // g2 is shorter than that from g1
  class GIDLenCompare {
    const BasicGroupSet& _gs;           // reference to the groupset
    unsigned _order;                    // order
  public:
    // order = 1 means longest first, order = 2 means shortest first
    GIDLenCompare(const BasicGroupSet& gs, unsigned order) 
      : _gs(gs), _order(order) {}
    bool operator()(GID g1, GID g2) {
      const BasicClauseVector& cls1 = _gs.gclauses(g1);
      unsigned sum1 = 0;
      for (BasicClauseVector::const_iterator pcl = cls1.begin(); 
           pcl != cls1.end(); ++pcl)
        if (!(*pcl)->removed())
          sum1 += (*pcl)->asize();
      const BasicClauseVector& cls2 = _gs.gclauses(g2);
      unsigned sum2 = 0;
      for (BasicClauseVector::const_iterator pcl = cls2.begin(); 
           pcl != cls2.end(); ++pcl)
        if (!(*pcl)->removed())
          sum2 += (*pcl)->asize();
      return ((_order == 1) ? (sum1 < sum2) : (sum1 > sum2));
    }
  };
  // priority queue using the comparator
  std::priority_queue<GID, std::vector<GID>, GIDLenCompare> _q;

public:

  /** order = 1 means longest first; order = 2 means shortest first */
  LengthScheduler(MUSData& md, unsigned order) 
    : GroupScheduler(md), _q(GIDLenCompare(md.gset(), order)) {
    // populate the queue
    for (gset_iterator pg = md.gset().gbegin(); pg != md.gset().gend(); ++pg) {
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

#endif // _LENGTH_SCHEDULER_HH

/*----------------------------------------------------------------------------*/
