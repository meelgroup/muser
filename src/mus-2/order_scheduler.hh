/*----------------------------------------------------------------------------*\
 * File:        order_scheduler.hh
 *
 * Description: Implementation of scheduler template that order groups
 *              using a user-provided comparator.
 *
 * Author:      antonb
 * 
 *                                               Copyright (c) 2013, Anton Belov
 \*----------------------------------------------------------------------------*/

#pragma once

#include "mtl/mheap.hh"
#include "basic_group_set.hh"
#include "group_scheduler.hh"
#include "mus_data.hh"

/*----------------------------------------------------------------------------*\
 * Class:  OrderScheduler
 *
 * Purpose: Scheduler for groups that orders groups using a used-provided
 *          comparator. The scheduler uses Minisat's heap, and so is dynamic.
 *          The comparator will be invoked as GIDCompare(GID g1, GID g2) which
 *          should return true if g1 < g2. Note that the scheduler is a
 *          *max-heap*, i.e. it gives out the largest elements first.
 *
 * Notes:
 *
\*----------------------------------------------------------------------------*/

template<typename GIDCompare = std::less<GID>>
class OrderScheduler : public GroupScheduler {

public: // main interface

  OrderScheduler(MUSData& md, const GIDCompare& comp = GIDCompare())
    : GroupScheduler(md), _heap(comp) {
    // populate the queue
    for (auto pg = _md.gset().gbegin(); pg != _md.gset().gend(); ++pg)
      if (*pg != 0)
        _heap.insert(*pg);
  }

  /** Returns true and sets the next group id for a given worker ID
   * [0,num_workers) if there's more groups; otherwise false
   */
  virtual bool next_group(GID& next_gid, unsigned worker_id = 0) {
    if (_heap.empty())
      return false;
    next_gid = _heap.removeMin();
    return true;
  }

  /** This allows users to re-schedule a group ID check - the invariant is that
   * after this call next_group will give out the gid at some point;
   */
  virtual void reschedule(GID gid) {
    _heap.insert(gid);
  }

  /** This method should be called whenever some value that affects the order
   * of group-ID gets changed.
   */
  virtual void update(GID gid) {
    _heap.update(gid);
  }

  /** This method should be called whenever some group-ID gets removed.
   */
  virtual void update_removed(GID gid) {
    // TODO: think how to implement this best; I think the new mtl has the
    // method to remove elements from the heap
  }

private:

  Minisat::Heap<GIDCompare> _heap;      // *mutable* heap

};

/*----------------------------------------------------------------------------*/
