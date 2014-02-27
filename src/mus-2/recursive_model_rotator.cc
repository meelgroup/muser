/*----------------------------------------------------------------------------*\
 * File:        recursive_model_rotator.cc
 *
 * Description: Implementation of model rotator that uses RMR (FMCAD-2011).
 *
 * Author:      antonb
 * 
 * Notes:
 *
 *   1. IMPORTANT: this implementation is NOT multi-thread safe, and NOT ready
 *      for use multi-threaded mode.
 *
 *
 *                                          Copyright (c) 2011-2013, Anton Belov
 \*----------------------------------------------------------------------------*/

#ifdef NDEBUG
//#undef NDEBUG // enable assertions (careful !)
#endif

#include <cassert>
#include <ext/hash_set>
#include <unordered_map>
#include <iostream>
#include <list>
#include <queue>
#include "basic_group_set.hh"
#include "types.hh"
#include "model_rotator.hh"
#include "utils.hh"

using namespace std;
using namespace __gnu_cxx;

//#define DBG(x) x

namespace {

  // This is an entry into the rotation queue.
  struct rot_queue_entry {
    GID _gid;               // the falsified GID
    list<ULINT> _delta;     // the delta from the original assignment (model)
    rot_queue_entry(void) {}
    rot_queue_entry(GID gid) : _gid(gid) {}
    rot_queue_entry(GID gid, list<ULINT>& delta) : _gid(gid), _delta(delta) {}
  };

} // anonymous namespace

/* Handles the RotateModel work item
 */
template<class Dec>
bool RecursiveModelRotatorTmpl<Dec>::process(RotateModel& rm)
{
  MUSData& md = rm.md();
  BasicGroupSet& gs = md.gset();
  OccsList& o_list = gs.occs_list();
  const IntVector& orig_model = rm.model();

#ifdef MULTI_THREADED
  throw logic_error("RecursiveModelRotatorTmpl::process() is not ready for multi-threaded mode.");
#endif

  DBG(cout << "+RecursiveModelRotatorTmpl::process(" << rm.gid() << ")" << endl;);

  // models are coded in terms of their deltas to the original model - a delta
  // is a list or set of variables in the original model that should be flipped;
  // a work queue entry consists of a group-set G (a set of group ids) and a 
  // and delta for it -- the invariant is that all of the groups in G are falsified
  // by the assignment represented by delta.
  queue<rot_queue_entry> rot_queue;

  // the first entry (delta is empty)
  rot_queue.push(rot_queue_entry(rm.gid()));
  // a copy of the original model -- will be used as working assignment.
  IntVector curr_ass(orig_model);
  while (!rot_queue.empty()) {
    rot_queue_entry& e = rot_queue.front();
    GID gid = e._gid;
    DBG(cout << "Rotating gid=" << gid;);

    // update working assignment and its hash (don't forget to restore)
    DBG(cout << ", delta: ";);
    for (list<ULINT>::iterator pv = e._delta.begin(); pv != e._delta.end(); ++pv) {
      DBG(cout << *pv << " ";);
      Utils::flip(curr_ass, *pv);
    }
    DBG(cout << endl;);

    // ok, we're ready to roll:
    //      for each modification to the model (currently just a single flip)
    //      calculate the set of falsified group ids; if the set becomes larger
    //      than one, abort; otherwise we've got another necessary group;

    // collect the set of variables that appear in the falsified clauses of gid
    IntSet cand_vars;
    const BasicClauseVector& gclauses = gs.gclauses(gid);
    for (cvec_citerator pcl = gclauses.begin(); pcl != gclauses.end(); ++pcl) {
      if ((*pcl)->removed()) 
        continue;
      else if (Utils::tv_clause(curr_ass, *pcl) == -1) {
        if ((*pcl)->asize() == 0) { // empty clause -- get out, can't do anything
          DBG(cout << "Saw an empty clause, can't do anything else." << endl;);
          goto _done;
        }
        for(CLiterator lpos = (*pcl)->abegin(); lpos != (*pcl)->aend(); ++lpos)
          cand_vars.insert(abs(*lpos));
      }       
    }
    assert(!cand_vars.empty()); // must be non-empty, o/w gid is not UNSAT !

    // now, for each candidate -- flip and calculate the set of falsified group
    // ids -- if its of size 1, it will go into the queue
    for (IntSet::iterator pv = cand_vars.begin(); pv != cand_vars.end(); ++pv) {
      LINT lit = *pv * curr_ass[*pv]; // clauses with lit might become falsified
      Utils::flip(curr_ass, *pv);
      DBG(cout << "  Checking var " << *pv << ", assigned " << curr_ass[*pv] << ": ";);
      GIDSet new_gids; // these will be falsified gids -- this is a plubming
                       // for the future work on dependencies
      // run through group clauses -- if any clause is still false, add gid to
      // new_gids;
      for (cvec_citerator pcl = gclauses.begin(); pcl != gclauses.end(); ++pcl) {
        if ((*pcl)->removed()) 
          continue;
        else if (Utils::tv_clause(curr_ass, *pcl) == -1) {
          new_gids.insert(gid);
          break;
        }
      }
      // ok, run through the clauses that contain lit, and for any clause that
      // *became* falsified due to the flip, add its gid also to new_gids
      if (new_gids.size() == 0) { // skip otherwise
        BasicClauseList& lclauses = o_list.clauses(lit);
        for (BasicClauseList::iterator pcl = lclauses.begin(); pcl != lclauses.end(); ) {
          if ((*pcl)->removed()) { // clause is removed, update occslist
            pcl = lclauses.erase(pcl);
            continue;
          }
          if (Utils::tv_clause(curr_ass, *pcl) == -1) {
            GID cand_gid = (*pcl)->get_grp_id();
            if ((cand_gid != 0) || !rm.ignore_g0())
              new_gids.insert(cand_gid);  
            // early break: if new_gids.size() > 1 -- get out
            if (new_gids.size() > 1)
              break;
          }   
          ++pcl; // next clause
        }
        // ok, now we have the falsified gid-set in new_gids; put them on the queue
        DBG(cout << "  Falsified gids: " << new_gids << endl;);
        // check if singleton and new - if yes, enqueue and add
        if (new_gids.size() == 1) {
          GID new_gid = *new_gids.begin();
          // consult the decider
          if (_d.rotate_through(rm, new_gid, lit)) {
            rm.nec_gids().insert(new_gid);
            rot_queue_entry r(new_gid, e._delta);
            r._delta.push_back(*pv);
            rot_queue.push(r);
            DBG(cout << " put on the queue" << endl;);
          }
        } else if (rm.collect_ft_gids()) {
          // remember the gids to fastrack
          copy(new_gids.begin(), new_gids.end(), inserter(rm.ft_gids(), rm.ft_gids().end()));
        }
      } // if (new_gids.size() == 0) ...
      // unflip
      Utils::flip(curr_ass, *pv);
    }
    // restore working model
    for (list<ULINT>::iterator pv = e._delta.begin(); pv != e._delta.end(); ++pv)
      Utils::flip(curr_ass, *pv);
    // done this group-set
    rot_queue.pop();
    _num_points++;
  } // while (queue)
 _done:
  rm.set_completed();
  if (rm.ignore_global()) { _d.clear(); }
  DBG(cout << "-RecursiveModelRotator::process(" << rm.gid() << ")" << endl;);
  return rm.completed();
}


/** Implementation of DeciderRMR::rotate_through --- pick into the globally
 * known set of necessary gids (unless said not to) and the local one
 */
bool DeciderRMR::rotate_through(RotateModel& rm, GID gid, LINT lit)
{
  return (rm.ignore_global() || !rm.md().nec(gid))
          && rm.nec_gids().find(gid) == rm.nec_gids().end();

}

/** Implementation of DeciderSMR::rotate_through -- keeps track of which literal
 * is used to arrive at the group. depth controls how many times can re-visit on
 * the same literal
 */
bool DeciderSMR::rotate_through(RotateModel& rm, GID gid, LINT lit)
{
  group_map::iterator pm = _gm.find(gid);
  if (pm == _gm.end()) { pm = _gm.emplace(make_pair(gid, lit_count_map())).first; }
  lit_count_map& lm = pm->second;
  lit_count_map::iterator pl = lm.find(lit);
  if (pl == lm.end()) { pl = lm.emplace(make_pair(lit, 0)).first; }
  unsigned vc = ++pl->second;
  DBG(cout << "gid=" << gid << ", lit=" << lit << ", vc = " << vc << endl;);
  return (vc <= _depth);
}

// this is to instantiate the templates -- allows to keep in the implementation
// in .cc file (C++11 feature); if you need another type, add it here
template class RecursiveModelRotatorTmpl<DeciderRMR>;
template class RecursiveModelRotatorTmpl<DeciderSMR>;

//
// ------------------------  Local implementations  ----------------------------
//

namespace {

} // anonymous namespace
