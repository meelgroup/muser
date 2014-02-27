/*----------------------------------------------------------------------------*\
 * File:        rgraph_scheduler.hh
 *
 * Description: Implementation of schedulers that order groups based on the 
 *              degree in resolution graph and its approximations based on
 *              occlists.
 *
 * Author:      antonb
 * 
 *                                               Copyright (c) 2012, Anton Belov
 \*----------------------------------------------------------------------------*/

#ifndef _RGRAPH_SCHEDULER_HH
#define _RGRAPH_SCHEDULER_HH 1

#include "basic_group_set.hh"
#include "order_scheduler.hh"
#include "mus_data.hh"
#include "utils.hh"


/* Comparator for GIDs using the degrees in the resolution graph; the template
 * paratemeter controls whether the comparator returns true if g1 < g2 or the
 * opposite. Assumes that the graph has been constructed.
 */
template<bool use_less = true>
class GIDRGraphCompare {

public:

  GIDRGraphCompare(const MUSData& md) : _md(md) {}
  
  bool operator()(GID g1, GID g2) const {
    const BasicGroupSet& gs = _md.gset();
    // works correctly for clauses only for now !
    assert(gs.gclauses(g1).size() == 1);
    assert(gs.gclauses(g2).size() == 1);
    assert(_md.has_rgraph());
    int deg1 = _md.rgraph().degree(*gs.gclauses(g1).begin());
    int deg2 = _md.rgraph().degree(*gs.gclauses(g2).begin());
    return (use_less) ? (deg1 < deg2) : (deg2 < deg1);
  }

private:

  const MUSData& _md;

};


/* Comparator for GIDs using the degrees in the implicit resolution graph -- that
 * is, it analyzes occslists. The template paratemeter controls whether the
 * comparator returns true if g1 < g2 or the opposite.
 */
template<bool use_less = true>
class GIDImplRGraphCompare {

public:

  GIDImplRGraphCompare(const MUSData& md) 
    : _md(md), _gdegs(md.gset().max_gid()+1,0) {}
  
  bool operator()(GID g1, GID g2) const {
    int& deg1 = _gdegs[g1];
    int& deg2 = _gdegs[g2];
    bool res;
    // if both are known, done
    if ((deg1 > 0) && (deg2 > 0))
      res = (use_less) ? (deg1 < deg2) : (deg2 < deg1);
    else {       
      if (use_less) {
        // for use_less, we can return true if deg1 upper bound is less than deg2
        if (deg1 == 0) deg1 = -Utils::cgraph_degree_approx(_md.gset(), g1);
        if (deg2 <= 0) deg2 = Utils::rgraph_degree(_md.gset(), g2);
        if ((deg1 < 0) && (abs(deg1) < deg2))
          res = true;
        else
          res = (deg1 = Utils::rgraph_degree(_md.gset(), g1)) < deg2; // now need real degree
      } else {
        // for use_more, we can return true if deg2 upperbound is less than deg1
        if (deg2 == 0) deg2 = -Utils::cgraph_degree_approx(_md.gset(), g2);
        if (deg1 <= 0) deg1 = Utils::rgraph_degree(_md.gset(), g1);
        if ((deg2 < 0) && (abs(deg2) < deg1))
          res = true;
        else
          res = (deg2 = Utils::rgraph_degree(_md.gset(), g2)) < deg1; // now need real degree
      }
    }
    // slow assertion and debug
    NDBG(cout << "deg[" << g1 << "]=" << deg1 << ", deg[" << g2 << "]=" << deg2 << endl; 
         cout << "real deg1: " << Utils::rgraph_degree(_md.gset(), g1)
              << ", deg2: " << Utils::rgraph_degree(_md.gset(), g2)
              << ", res=" << res << endl;);
    CHK(assert((use_less) 
               ? (res == (Utils::rgraph_degree(_md.gset(), g1) < Utils::rgraph_degree(_md.gset(), g2)))
               : (res == (Utils::rgraph_degree(_md.gset(), g2) < Utils::rgraph_degree(_md.gset(), g1)))));
    return res;
  }
  
private:

  const MUSData& _md;           // MUSData for this instance

  mutable vector<int> _gdegs;   // degrees hash 0 - unknown; > 0 - real degree; 
                                // < 0 - overapprox
};


/* Comparator for GIDs using the degrees in the implicit conflict graph -- that
 * is, it does not use the graph, but instead analyzes occslists. The template
 * paratemeter controls whether the comparator returns true if g1 < g2 or the
 * opposite.
 */
template<bool use_less = true>
class GIDImplCGraphCompare {

public:

  GIDImplCGraphCompare(const MUSData& md) 
    : _md(md), _gdegs(md.gset().max_gid()+1,0) {}
  
  bool operator()(GID g1, GID g2) const {
    int& deg1 = _gdegs[g1];
    if (!deg1) deg1 = Utils::cgraph_degree_approx(_md.gset(), g1);
    int& deg2 = _gdegs[g2];
    if (!deg2) deg2 = Utils::cgraph_degree_approx(_md.gset(), g2);
    return (use_less) ? (deg1 < deg2) : (deg2 < deg1);
  }
  
private:

  const MUSData& _md;           // MUSData for this instance

  mutable vector<int> _gdegs;   // degrees hash 0 - unknown; > 0 - real degree; 

};



//
// scheduler class definitions
//

/* Scheduler that gives out GIDs with max degree first
 */
class RGraphSchedulerMax :
  public OrderScheduler<GIDRGraphCompare<false>> {

 public:
  RGraphSchedulerMax(MUSData& md) :
    OrderScheduler<GIDRGraphCompare<false>>(md, GIDRGraphCompare<false>(md)) {}

  /** This method should be called whenever some group-ID gets removed.
   */
  virtual void update_removed(GID gid) { 
    // what we want to do here is to get the neighbours of the removed clause
    // from the graph, and update the scheduler
    for (const BasicClause* cl : _md.rgraph().removed_nhood())
      update(cl->get_grp_id());
    OrderScheduler<GIDRGraphCompare<false>>::update_removed(gid);
  }

};

/* Scheduler that gives out GIDs with min degree first
 */
class RGraphSchedulerMin :
  public OrderScheduler<GIDRGraphCompare<true>> {

 public:
  RGraphSchedulerMin(MUSData& md) :
    OrderScheduler<GIDRGraphCompare<true>>(md, GIDRGraphCompare<true>(md)) {}

  /** This method should be called whenever some group-ID gets removed.
   */
  virtual void update_removed(GID gid) { 
    // what we want to do here is to get the neighbours of the removed clause
    // from the graph, and update the scheduler
    for (const BasicClause* cl : _md.rgraph().removed_nhood())
      update(cl->get_grp_id());
    OrderScheduler<GIDRGraphCompare<true>>::update_removed(gid);
  }

};

/* Scheduler that gives out GIDs with max degree first based on implicit
 * resolution graph
 */
class ImplRGraphSchedulerMax :
  public OrderScheduler<GIDImplRGraphCompare<false>> {

 public:
  ImplRGraphSchedulerMax(MUSData& md) :
    OrderScheduler<GIDImplRGraphCompare<false>>(md, GIDImplRGraphCompare<false>(md)) {}

};

/* Scheduler that gives out GIDs with min degree first based on implicit
 * resolution graph
 */
class ImplRGraphSchedulerMin :
  public OrderScheduler<GIDImplRGraphCompare<true>> {

 public:
  ImplRGraphSchedulerMin(MUSData& md) :
    OrderScheduler<GIDImplRGraphCompare<true>>(md, GIDImplRGraphCompare<true>(md)) {}

};

/* Scheduler that gives out GIDs with max degree first based on implicit
 * conflict graph
 */
class ImplCGraphSchedulerMax :
  public OrderScheduler<GIDImplCGraphCompare<false>> {

 public:
  ImplCGraphSchedulerMax(MUSData& md) :
    OrderScheduler<GIDImplCGraphCompare<false>>(md, GIDImplCGraphCompare<false>(md)) {}

};

/* Scheduler that gives out GIDs with min degree first based on implicit
 * conflict graph
 */
class ImplCGraphSchedulerMin :
  public OrderScheduler<GIDImplCGraphCompare<true>> {

 public:
  ImplCGraphSchedulerMin(MUSData& md) :
    OrderScheduler<GIDImplCGraphCompare<true>>(md, GIDImplCGraphCompare<true>(md)) {}

};





#endif // _RGRAPH_SCHEDULER_HH

/*----------------------------------------------------------------------------*/
