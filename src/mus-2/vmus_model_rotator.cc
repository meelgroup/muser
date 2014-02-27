/*----------------------------------------------------------------------------*\
 * File:        vmus_model_rotator.cc
 *
 * Description: Implementation of model rotator for VMUS compuation.
 *
 * Author:      antonb
 * 
 * Notes:
 *
 *   1. IMPORTANT: this implementation is NOT multi-thread safe, and NOT ready
 *      for use multi-threaded mode.
 *
 *
 *
 *                                              Copyright (c) 2012, Anton Belov
 \*----------------------------------------------------------------------------*/

#ifdef NDEBUG
#undef NDEBUG // enable assertions (careful !)
#endif

#include <cassert>
#include <ext/hash_set>
#include <iostream>
#include <list>
#include <queue>
#include "basic_group_set.hh"
#include "types.hh"
#include "model_rotator.hh"

using namespace std;
using namespace __gnu_cxx;

//#define DBG(x) x
//#define CHK(x) x

namespace {

  // This is an entry into the rotation queue. Important invariant: all clauses
  // falsified by _delta have a variable from group _vgid
  struct rot_queue_entry {
    GID _vgid;              // the falsified VGID
    list<ULINT> _delta;     // the delta from the original assignment (model)
    rot_queue_entry(void) {}
    rot_queue_entry(GID vgid) : _vgid(vgid) {}
    rot_queue_entry(GID vgid, list<ULINT>& delta) : _vgid(vgid), _delta(delta) {}
  };

  // Flips a variable in the assignment
  void flip(IntVector& ass, ULINT var);

  // Returns the truth-value of clause under assignment: -1;0:+1
  int tv_clause(const IntVector& ass, const BasicClause* cl);

  // Checks whether a given assignment satisfies a set of clauses; return 1 for SAT, -1
  // for UNSAT, 0 for undetermined. A set is SAT iff all clauses are SAT, a set
  // is UNSAT iff at least one clause is UNSAT, undetermined otherwise
  int tv_group(const IntVector& ass, const BasicClauseVector& clauses);

  // Collects all clauses with variable var that are falsified by the given 
  // assignment
  void get_f_clauses(const IntVector& ass, BasicGroupSet& gs, ULINT var, 
                     HashedClauseSet& f_clauses);

} // anonymous namespace

/* Handles the RotateModel work item
 *
 * TODO: in EMR mode, the main loop maybe executed many times, and so on some 
 * instances it becomes the major player (e.g. UTI-20-5t0.cnf.gz in -emr mode
 * hits 1.17Mpoints, while in normal mode only 32K; in -emr mode SAT solving
 * time is only about 40% of total, whereas in normal mode its about 90%); 
 * perhaps its worth a bit of time on optimizing the loop.
 *
 */
bool VMUSModelRotator::process(RotateModel& rm)
{
  MUSData& md = rm.md();
  BasicGroupSet& gs = md.gset();
  const IntVector& orig_model = rm.model();

#ifdef MULTI_THREADED
  throw logic_error("VMUSModelRotator::process() is not ready for multi-threaded mode.");
#endif

  DBG(cout << "+VMUSModelRotator::process(" << rm.gid() << ")" << endl;);

  // models are coded in terms of their deltas to the original model - a delta
  // is a list or set of variables in the original model that should be flipped;
  // a work queue entry consists of a vgid and a and delta for it
  queue<rot_queue_entry> rot_queue;
  
  // local history (value = count of visits)
  GID2IntMap visited;
  unsigned rot_depth = rm.rot_depth(); // 0 means use global info (aka rmr)

  // the first entry (delta is empty)
  rot_queue.push(rot_queue_entry(rm.gid()));
  // a copy of the original model -- will be used as working assignment.
  IntVector curr_ass(orig_model);
  while (!rot_queue.empty()) {
    rot_queue_entry& e = rot_queue.front();
    GID vgid = e._vgid;
    DBG(cout << "Rotating vgid=" << vgid;);

    // update working assignment and its hash (don't forget to restore)
    DBG(cout << ", delta: ";);
    for (list<ULINT>::iterator pv = e._delta.begin(); pv != e._delta.end(); ++pv) {
      DBG(cout << *pv << " ";);
      flip(curr_ass, *pv);
    }
    DBG(cout << endl;);

    // compute the set of falsified clauses -- the invariant is that these can 
    // only be clauses with variables from vgid
    const VarVector& vars = gs.vgvars(vgid);
    HashedClauseSet f_clauses;
    for (VarVector::const_iterator pvar = vars.begin(); pvar != vars.end(); ++pvar)
      get_f_clauses(curr_ass, gs, *pvar, f_clauses);
    DBG(cout << "Falsified clauses (" << f_clauses.size() << "): ";
        PRINT_PTR_ELEMENTS(f_clauses););
    assert(f_clauses.size() >= 1);

    // expensive check of the invariant, make sure CHK is disabled in experiments
    CHK({// run through all clauses -- each falsified clause must be in the 
        // f_clauses vector, otherwise the invariant is violated.
        HashedClauseSet f_cls;
        copy(f_clauses.begin(), f_clauses.end(), inserter(f_cls, f_cls.end()));
        for (cvec_iterator pcl = gs.begin(); pcl != gs.end(); ++pcl) {
          if (!(*pcl)->removed() && (tv_clause(curr_ass, *pcl) == -1) 
              && (f_cls.find(*pcl) == f_cls.end())) {
            DBG(cout << "Spurious falsified clause: " << **pcl << endl;);
            tool_abort("VMUSModelRotator::process() -- rotation invariant is violated !");
          }
        }});

    // now, any variable group that appears in *all* false clauses is necessary; 
    // this of course includes vgid itself
    typedef __gnu_cxx::hash_map<GID, HashedClauseSet, IntHash, IntEqual> GIDClSetMap;
    GIDClSetMap f_vgids; // key = VGID, value = set of false clauses that have a
                         // variable from VGID
    for (HashedClauseSet::iterator pcl = f_clauses.begin(); 
         pcl != f_clauses.end(); ++pcl) {
      for (CLiterator plit = (*pcl)->abegin(); plit != (*pcl)->aend(); ++plit)
        f_vgids[gs.get_var_grp_id(abs(*plit))].insert(*pcl);
    }
    GIDSet new_vgids;
    for (GIDClSetMap::iterator pm = f_vgids.begin(); pm != f_vgids.end(); ++pm)
      if (pm->second.size() == f_clauses.size()) // all false clauses have it
        new_vgids.insert(pm->first);
    DBG(cout << "New_vgids: " << new_vgids << endl;);

    // now things are simple: for each new vgid, if its not known to be necessary
    // yet (or maybe other, counter-based criterion), make it necessary and queue
    // it and its delta (meaning: those vars that are in the false clauses).
    for (GIDSet::iterator pvgid = new_vgids.begin(); pvgid != new_vgids.end(); ++pvgid) {
      GID new_gid = *pvgid;
      DBG(cout << "  vgid " << new_gid << " is ");
      // this is the criterion (depends on the depth: 0 means use global info)
      bool queue_it = (rot_depth == 0) 
        ? (new_gid && !md.nec(new_gid) && (rm.nec_gids().find(new_gid) == rm.nec_gids().end()))
        : (new_gid && (e._delta.empty() || (new_gid != *e._delta.rbegin())) && (visited[new_gid] < (int)rot_depth));
      if (queue_it) {
        rm.nec_gids().insert(new_gid);
        if (rot_depth > 0) visited[new_gid]++;
        rot_queue_entry r(new_gid, e._delta);
        // fast path: new_gid is a singleton, just add it to delta
        // slow path: new_gid is not a singleton - only add those variables that
        //            actually appear in false clauses
        VarVector& vgvars = gs.vgvars(new_gid);
        assert(!vgvars.empty());
        if (vgvars.size() == 1) {
          r._delta.push_back(*vgvars.begin());
        } else {
          // now, we add to delta all of the variables from new_gid that appear 
          // in falsified clauses
          HashedClauseSet& f_set = f_vgids[new_gid];
          ULINTSet d_vars;
          for (ClSetIterator pcl = f_set.begin(); pcl != f_set.end(); ++pcl) {
            for (CLiterator plit = (*pcl)->abegin(); plit != (*pcl)->aend(); ++plit) {
              ULINT var = abs(*plit);
              if (gs.get_var_grp_id(var) == new_gid)
                d_vars.insert(var);
            }
          }
          copy(d_vars.begin(), d_vars.end(), back_inserter(r._delta));
        }
        rot_queue.push(r);
        DBG(if (rot_depth == 0)
              cout << "new (passed criterion)";
            else
              cout << "new (passed criterion, visited = " << visited[new_gid] << ")";
            cout << ", put on the queue." << endl;);
      } else {
        DBG(cout << "old (didn't pass criterion), skipped." << endl;);
      }
    }

    // restore working model
    for (list<ULINT>::iterator pv = e._delta.begin(); pv != e._delta.end(); ++pv)
      flip(curr_ass, *pv);
    // done this group-set
    rot_queue.pop();
    _num_points++;
  } // while (queue)
  rm.set_completed();
  DBG(cout << "-VMUSModelRotator::process(" << rm.gid() << ")" << endl;);
  return rm.completed();
}



//
// ------------------------  Local implementations  ----------------------------
//

namespace {

  // Returns the truth-value of clause under assignment: -1;0:+1
  int tv_clause(const IntVector& ass, const BasicClause* cl)
  {
    unsigned false_count = 0;
    for(CLiterator lpos = cl->abegin(); lpos != cl->aend(); ++lpos) {
      int var = abs(*lpos);
      if (ass[var]) {
        if ((*lpos > 0 && ass[var] == 1) ||
            (*lpos < 0 && ass[var] == -1))
          return 1;
        false_count++;
      }
    }
    return (false_count == cl->asize()) ? -1 : 0;
  }

  // Checks whether a given assignment satisfies a set of clauses; return 1 for SAT, -1
  // for UNSAT, 0 for undetermined. A set is SAT iff all clauses are SAT, a set
  // is UNSAT iff at least one clause is UNSAT, undetermined otherwise
  int tv_group(const IntVector& ass, const BasicClauseVector& clauses)
  {
    unsigned sat_count = 0;
    for (cvec_citerator pcl = clauses.begin(); pcl != clauses.end(); ++pcl) {
      int tv = tv_clause(ass, *pcl);
      if (tv == -1)
        return -1;
      sat_count += tv;
    }
    return (sat_count == clauses.size()) ? 1 : 0;
  }

  // Flips a variable in the assignment
  void flip(IntVector& ass, ULINT var) {
    LINT val = ass[var];
    assert(val != 0); // really shouldn't be flipping unassigned vars
    if (val)
      ass[var] = (val == 1) ? -1 : 1;
  }

  // Collects all clauses with variable var that are falsified by the given 
  // assignment
  void get_f_clauses(const IntVector& ass, BasicGroupSet& gs, ULINT var, 
                     HashedClauseSet& f_clauses) 
  {
    OccsList& o_list = gs.occs_list();
    BasicClauseList& cp = o_list.clauses(var);
    for (BasicClauseList::iterator pcl = cp.begin(); pcl != cp.end(); ) {
      if ((*pcl)->removed()) { 
        pcl = cp.erase(pcl); continue; 
      }
      if (tv_clause(ass, *pcl) == -1)
        f_clauses.insert(*pcl);
      ++pcl;
    }
    BasicClauseList& cn = o_list.clauses(-var);
    for (BasicClauseList::iterator pcl = cn.begin(); pcl != cn.end(); ) {
      if ((*pcl)->removed()) { 
        pcl = cp.erase(pcl); continue; 
      }
      if (tv_clause(ass, *pcl) == -1)
        f_clauses.insert(*pcl);
      ++pcl;
    }
  }

} // anonymous namespace
