/*----------------------------------------------------------------------------*\
 * File:        order_scheduler.hh
 *
 * Description: Implementation of scheduler templates that order groups
 *              using a user-provided comparator. The first scheduler orders
 *              orders elements during contruction, and so is static, the
 *              second uses STL priority queue, and so is almost static (i.e. not 
 *              reordered when priorities change, but when groups are removed
 *              or rescheduled the comparator is used), the third scheduler 
 *              uses Boost's mutable heap, so that the order can be changed 
 *              dynamically.
 *
 * Author:      antonb
 * 
 *                                               Copyright (c) 2012, Anton Belov
 \*----------------------------------------------------------------------------*/

#ifndef _ORDER_SCHEDULER_HH
#define _ORDER_SCHEDULER_HH 1

#include <algorithm>
#include <boost/heap/fibonacci_heap.hpp>
#include <queue>
#include <unordered_map>
#include "basic_group_set.hh"
#include "group_scheduler.hh"
#include "mus_data.hh"

/*----------------------------------------------------------------------------*\
 * Class:  StaticOrderScheduler
 * 
 * Purpose: Scheduler for groups that orders groups using a used-provided 
 *          comparator. The scheduler orders GIDs during construction, and
 *          does not change the order anymore. reschedule() puts GIDs to the
 *          end of the queue. The comparator will be invoked as 
 *          GIDCompare(GID g1, GID g2) which should return true if g1 < g2. 
 *          Note that the scheduler is a *max-heap*, i.e. it gives out 
 *          the largest elements first.
 *
 * Notes:
 *
\*----------------------------------------------------------------------------*/

template<class GIDCompare = std::less<GID>>
class StaticOrderScheduler : public GroupScheduler {

public: // main interface

  StaticOrderScheduler(MUSData& md, const GIDCompare& comp = GIDCompare())
    : GroupScheduler(md) {
    // populate the queue
    std::vector<GID> v;
    for (auto pg = md.gset().gbegin(); pg != md.gset().gend(); ++pg)
      if (*pg != 0)
        v.push_back(*pg);
    std::stable_sort(v.begin(), v.end(), comp); // descending order
    for (GID gid : v) 
      if (gid) _q.push_front(gid);
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
   * after this call next_group will give out the gid at some point;
   */
  virtual void reschedule(GID gid) {
    _q.push_back(gid);
  }

private:

  std::deque<GID> _q;       // queue with GIDs - give one at a time

};


/*----------------------------------------------------------------------------*\
 * Class:  OrderScheduler
 * 
 * Purpose: Scheduler for groups that orders groups using a used-provided 
 *          comparator. The scheduler uses STL priority queue, and so is 
 *          semi-static (the comparator will be probably called on 
 *          next_group() and reschedule(), but if some element changes order
 *          while in the heap, it will stay where it is). The comparator will
 *          be invoked as GIDCompare(GID g1, GID g2) which should return true if 
 *          g1 < g2. Note that the scheduler is a *max-heap*, i.e. it gives out 
 *          the largest elements first.
 *
 * Notes:
 *
\*----------------------------------------------------------------------------*/

template<class GIDCompare = std::less<GID>>
class OrderScheduler : public GroupScheduler {

public: // main interface

  OrderScheduler(MUSData& md, const GIDCompare& comp = GIDCompare())
    : GroupScheduler(md), _q(comp) {
    // populate the queue
    for (auto pg = _md.gset().gbegin(); pg != _md.gset().gend(); ++pg)
      if (*pg != 0)
        _q.push(*pg);
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

private:

  // priority queue
  std::priority_queue<GID, std::vector<GID>, GIDCompare> _q;

};



/*----------------------------------------------------------------------------*\
 * Class:  DynamicOrderScheduler
 * 
 * Purpose: Scheduler for groups that orders groups using a used-provided 
 *          comparator. The scheduler uses mutable heap, so that the order can
 *          be changed dynamically. The user is expected to notify the scheduler
 *          whenever a priority of some element changes by calling update(). 
 *          The comparator will be invoked as GIDCompare(GID g1, GID g2) which 
 *          should return true if g1 < g2. Note that the scheduler is a *max-heap*, 
 *          i.e. it gives out the largest elements first.
 *
 * Notes:
 *
\*----------------------------------------------------------------------------*/

template<class GIDCompare = std::less<GID>>
class DynamicOrderScheduler : public GroupScheduler {

  // types
  typedef boost::heap::fibonacci_heap<GID, boost::heap::compare<GIDCompare>> MHeap;
  typedef typename MHeap::handle_type MHeapHandle;
  typedef std::unordered_map<GID, MHeapHandle> HandleMap;

public: // main interface

  DynamicOrderScheduler(MUSData& md, const GIDCompare& comp = GIDCompare())
    : GroupScheduler(md), _q(comp) {
    // populate the queue
    for (auto pg = _md.gset().gbegin(); pg != _md.gset().gend(); ++pg)
      if (*pg != 0)
        _hm[*pg] = _q.push(*pg);
  }     

  /** Returns true and sets the next group id for a given worker ID
   * [0,num_workers) if there's more groups; otherwise false
   */
  virtual bool next_group(GID& next_gid, unsigned worker_id = 0) {
    if (_q.empty())
      return false;
    next_gid = _q.top();
    _q.pop();
    auto phm = _hm.find(next_gid);
    assert(phm != _hm.end());
    if (phm != _hm.end()) _hm.erase(phm);
    return true;
  }

  /** This allows users to re-schedule a group ID check - the invariant is that
   * after this call next_group will give out the gid at some point;
   */
  virtual void reschedule(GID gid) {
    _hm[gid] = _q.push(gid);
  }

  /** This method should be called whenever some group-ID gets removed.
   */
  virtual void update_removed(GID gid) { 
    auto phm = _hm.find(gid);
    if (phm != _hm.end())
      _q.erase(phm->second);
  }

  /** This method should be called whenever some value that affects the order
   * of group-ID gets changed.
   */
  virtual void update(GID gid) { 
    auto phm = _hm.find(gid);
    if (phm != _hm.end())
      _q.update(phm->second);
  }

protected:
  
  MHeap  _q;   // the queue itself

  HandleMap _hm; // handle map GID -> handle

};

#endif // _ORDER_SCHEDULER_HH

/*----------------------------------------------------------------------------*/
