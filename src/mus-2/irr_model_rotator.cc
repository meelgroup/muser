/*----------------------------------------------------------------------------*\
 * File:        irr_model_rotator.cc
 *
 * Description: Implementation of special model rotator that can be used to 
 *              compute irredundant subformulas of CNF formulas (not only UNSAT).
 *
 * Author:      antonb
 * 
 * Notes:
 *
 *   1. IMPORTANT: this implementation is NOT multi-thread safe, and NOT ready
 *      for use multi-threaded mode.
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

namespace {

  // This is an entry into the rotation queue.
  struct rot_queue_entry {
    GID _gid;               // the falsified GID
    list<ULINT> _delta;     // the delta from the original assignment (model)
    rot_queue_entry(void) {}
    rot_queue_entry(GID gid) : _gid(gid) {}
    rot_queue_entry(GID gid, list<ULINT>& delta) : _gid(gid), _delta(delta) {}
  };

  // The set of visited SAT GIDs
  GIDSet visited_sat;

  // Flips a variable in the assignment
  void flip(IntVector& ass, ULINT var);

  // Returns the truth-value of clause under assignment: -1;0:+1
  int tv_clause(IntVector& ass, const BasicClause* cl);

  // Returns the truth-value of clause under assignment: -1;0:+num_true_lit
  int tv_clause_slow(IntVector& ass, const BasicClause* cl);

  // Checks whether a given assignment satisfies a set of clauses; return 1 for SAT, -1
  // for UNSAT, 0 for undetermined. A set is SAT iff all clauses are SAT, a set
  // is UNSAT iff at least one clause is UNSAT, undetermined otherwise
  int tv_group(IntVector& ass, const BasicClauseVector& clauses);

} // anonymous namespace

/* Handles the RotateModel work item
 */
bool IrrModelRotator::process(RotateModel& rm)
{
  MUSData& md = rm.md();
  BasicGroupSet& gs = md.gset();
  OccsList& o_list = gs.occs_list();
  const IntVector& orig_model = rm.model();

#ifdef MULTI_THREADED
  throw logic_error("IrrModelRotator::process() is not ready for multi-threaded mode.");
#endif

  DBG(cout << "+IrrModelRotator::process(" << rm.gid() << ")" << endl;);

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
    DBG(cout << ": " << **(gs.gclauses(gid).begin()););

    // update working assignment and its hash (don't forget to restore)
    DBG(cout << ", delta: ";);
    for (list<ULINT>::iterator pv = e._delta.begin(); pv != e._delta.end(); ++pv) {
      DBG(cout << *pv << " ";);
      flip(curr_ass, *pv);
    }
    DBG(cout << ", invariant: " << flush;
        // gid has to be *the only* false gid
        unsigned fcount = 0; GID fgid = gid_Undef;
        for (gset_iterator pgid = gs.gbegin(); pgid != gs.gend(); ++pgid) {
          if ((!md.r(*pgid)) && (tv_group(curr_ass, gs.gclauses(*pgid)) == -1)) {
            fgid = *pgid;
            fcount++;
          }
        }
        cout << fcount;
        if ((fcount > 1) || ((fcount == 1) && (gid != fgid))) {
          cout << "Invariant violated !!!" << endl; 
          abort();
        }
        cout << endl;);

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
      else if (tv_clause(curr_ass, *pcl) == -1) {
        for(CLiterator lpos = (*pcl)->abegin(); lpos != (*pcl)->aend(); ++lpos)
          cand_vars.insert(abs(*lpos));
      }       
    }
    // +TEMP: handling of the SAT case
    if (cand_vars.empty()) {
      // now, collect the variables that occur in once-SAT clauses of gid
      for (cvec_citerator pcl = gclauses.begin(); pcl != gclauses.end(); ++pcl) {
        if ((*pcl)->removed()) 
          continue;
        else if (tv_clause_slow(curr_ass, *pcl) == 1) {
          for(CLiterator lpos = (*pcl)->abegin(); lpos != (*pcl)->aend(); ++lpos)
            cand_vars.insert(abs(*lpos));
        }       
      }
    }
    // -TEMP
    assert(!cand_vars.empty()); // must be non-empty, o/w gid is not UNSAT !

    // now, for each candidate -- flip and calculate the set of falsified group
    // ids -- if its of size 1, it will go into the queue
    for (IntSet::iterator pv = cand_vars.begin(); pv != cand_vars.end(); ++pv) {
      LINT lit = *pv * curr_ass[*pv]; // clauses with lit might become falsified
      flip(curr_ass, *pv);
      DBG(cout << "  Checking var " << *pv << ", assigned " << curr_ass[*pv] << ": ";);
      GIDSet new_gids; // these will be falsified gids -- this is a plubming
                       // for the future work on dependencies
      // run through group clauses -- if any clause is still false, add gid to
      // new_gids;
      for (cvec_citerator pcl = gclauses.begin(); pcl != gclauses.end(); ++pcl) {
        if ((*pcl)->removed()) 
          continue;
        else if (tv_clause(curr_ass, *pcl) == -1) {
          new_gids.insert(gid);
          break;
        }
      }
      // ok, run through the clauses that contain lit, and for any clause that
      // *became* falsified due to the flip, add its gid also to new_gids
      BasicClauseList& lclauses = o_list.clauses(lit);
      for (BasicClauseList::iterator pcl = lclauses.begin(); pcl != lclauses.end(); ) {
        if ((*pcl)->removed()) { // clause is removed, update occslist
          pcl = lclauses.erase(pcl);
          continue;
        }
        if (tv_clause(curr_ass, *pcl) == -1) {
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
        // here we have an option to pick into the globally known set of 
        // necessary gids, as well as the local one - we'll do both
        if (!md.nec(new_gid) && rm.nec_gids().find(new_gid) == rm.nec_gids().end()) {
          rm.nec_gids().insert(new_gid);
          rot_queue_entry r(new_gid, e._delta);
          r._delta.push_back(*pv);
          rot_queue.push(r);
          DBG(cout << " put on the queue" << endl;);
        }
      } else if ((new_gids.size() > 1) && rm.collect_ft_gids()) {
        // remember the gids to fastrack
        copy(new_gids.begin(), new_gids.end(), inserter(rm.ft_gids(), rm.ft_gids().end()));
      } 
      // +TEMP: rotate through SAT clauses
      else if (new_gids.size() == 0) {
        // all groups will become satisfied ... ok, pick one that will be 
        // satisfied critically, and that 
        for (BasicClauseList::iterator pcl = lclauses.begin(); pcl != lclauses.end(); ) {
          // no need to update occlist -- already done
          if (tv_clause_slow(curr_ass, *pcl) == 1) {
            GID cand_gid = (*pcl)->get_grp_id();
            if ((cand_gid != 0) || !rm.ignore_g0()) {
              // check if visited already
              if (!md.nec(cand_gid) &&         
                  rm.nec_gids().find(cand_gid) == rm.nec_gids().end() && 
                  visited_sat.find(cand_gid) == visited_sat.end()) {
                new_gids.insert(cand_gid);  
                visited_sat.insert(cand_gid); // TEMP: move to queuing logic
              }
            }
            // early break: if new_gids.size() == 1 -- get out
            //if (new_gids.size() >= 3)
            //  break;
          }   
          ++pcl; // next clause
        }
        DBG(cout << "  Once-sat gids: " << new_gids << endl;);  
        // queue
        for (GIDSet::iterator pgid = new_gids.begin(); pgid != new_gids.end(); ++pgid) {
          rot_queue_entry r(*pgid, e._delta);
          r._delta.push_back(*pv);
          rot_queue.push(r);
          DBG(cout << " put on the queue" << endl;);
        }
      }
      // -TEMP
      // unflip
      flip(curr_ass, *pv);
    }
    // restore working model
    for (list<ULINT>::iterator pv = e._delta.begin(); pv != e._delta.end(); ++pv)
      flip(curr_ass, *pv);
    // done this group-set
    rot_queue.pop();
  } // while (queue)
  rm.set_completed();
  DBG(cout << "-IrrModelRotator::process(" << rm.gid() << ")" << endl;);
  return rm.completed();
}


//
// ------------------------  Local implementations  ----------------------------
//

namespace {

  // Returns the truth-value of clause under assignment: -1;0:+1
  int tv_clause(IntVector& ass, const BasicClause* cl)
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

  // Returns the truth-value of clause under assignment: 
  // -1; 0; +<number of true literals>
  int tv_clause_slow(IntVector& ass, const BasicClause* cl)
  {
    unsigned false_count = 0, true_count = 0;
    for(CLiterator lpos = cl->abegin(); lpos != cl->aend(); ++lpos) {
      int var = abs(*lpos);
      if (ass[var]) {
        if ((*lpos > 0 && ass[var] == 1) ||
            (*lpos < 0 && ass[var] == -1))
          true_count++;
        else
          false_count++;
      }
    }
    return (true_count > 0) ? true_count : ((false_count == cl->asize()) ? -1 : 0);
  }

  // Checks whether a given assignment satisfies a set of clauses; return 1 for SAT, -1
  // for UNSAT, 0 for undetermined. A set is SAT iff all clauses are SAT, a set
  // is UNSAT iff at least one clause is UNSAT, undetermined otherwise
  int tv_group(IntVector& ass, const BasicClauseVector& clauses)
  {
    unsigned a_count = 0, sat_count = 0;
    for (cvec_citerator pcl = clauses.begin(); pcl != clauses.end(); ++pcl) {
      if ((*pcl)->removed())
        continue;
      a_count++;
      int tv = tv_clause(ass, *pcl);
      if (tv == -1)
        return -1;
      sat_count += tv;
    }
    return (sat_count == a_count) ? 1 : 0;
  }

  // Flips a variable in the assignment
  void flip(IntVector& ass, ULINT var) {
    LINT val = ass[var];
    assert(val != 0); // really shouldn't be flipping unassigned vars
    if (val)
      ass[var] = (val == 1) ? -1 : 1;
  }

} // anonymous namespace
