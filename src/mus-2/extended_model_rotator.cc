/*----------------------------------------------------------------------------*\
 * File:        extended_model_rotator.cc
 *
 * Description: Implementation of model rotator that uses EMR (AIComm-11).
 *
 * Author:      antonb
 * 
 * Notes:
 *
 *   1. IMPORTANT: this implementation is NOT multi-thread safe, and NOT ready
 *      for use multi-threaded mode.
 *
 *
 *                                              Copyright (c) 2011, Anton Belov
 \*----------------------------------------------------------------------------*/

#ifdef NDEBUG
//#undef NDEBUG // enable assertions (careful !)
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

  // This is an entry into the rotation queue. Note that the memory impact of
  // this can be optimized (via hashing) -- for now, treat it as a prototype
  struct rot_queue_entry {
    GIDSet _gids;           // the set of falsified GIDs
    list<ULINT> _delta;     // the delta from the original assignment (model)
    rot_queue_entry(void) {}
    rot_queue_entry(GID gid) { _gids.insert(gid); }
    rot_queue_entry(GIDSet& gids, list<ULINT>& delta) : _gids(gids), _delta(delta) {}
  };

  // This is a type used as hash for assignments
  typedef unsigned long AHASH;

  // Set of hashes
  typedef hash_set<AHASH> AHASHSet;

  // This is the map from a set of GIDs to the set of hashes of its distinguishing 
  // assignments
  typedef hash_map<GIDSet, AHASHSet, GIDSetHash> DAMap;
  DAMap da_map;

  // Calculates and returns the hash-value of an assignment
  AHASH ass_hash(const IntVector& ass);
  
  // Updates the hash-value to match a flip of the specified variable
  void ass_hash_flip(AHASH& ass_hash, ULINT var);

  // Flips a variable in the assignment
  void flip(IntVector& ass, ULINT var);

  // Returns the truth-value of clause under assignment: -1;0:+1
  int tv_clause(IntVector& ass, const BasicClause* cl);

  // Checks whether a given assignment satisfies a set of clauses; return 1 for SAT, -1
  // for UNSAT, 0 for undetermined. A set is SAT iff all clauses are SAT, a set
  // is UNSAT iff at least one clause is UNSAT, undetermined otherwise
  int tv_group(IntVector& ass, const BasicClauseVector& clauses);
  
  // This is the decision-maker on whether to rotate through a groupset or not;
  // If true - the groupset will be rotated at the given assignment (i.e. the
  // neighbours of the assignment will be added to the queue); o/w not; note that 
  // this (with the help of da_map) superseedes the checks for whether or not a 
  // group is locally or globally necessary, because *all* necessary groups will 
  // end up in da_map
  bool allow_to_rotate(GIDSet& gids, IntVector& ass, AHASH& hash, 
                       unsigned rdepth, unsigned rwidth)
  {
    DBG(cout << "  allow_to_rotate(): checking gids=" << gids
        << " at ass_hash 0x" << hex << hash << dec << " ... ";);
    // the current algorithm is as follows: do not allow to rotate if one of
    // the following conditions is satisfied:
    //   1. gids.size() > rotation width
    //   2. the assignment ass is already visited
    //   3. if we already visited 'rotation depth' or less distinguishing 
    //      assignment for gids

    // block anything over certain size (rotation width)
    if (rwidth && (gids.size() > rwidth)) {
      DBG(cout << "group-set too large - do not rotate." << endl;);
      return false;
    }
    DAMap::iterator ps = da_map.find(gids);
    // if gids is not in da_map - then, rotate (b/c we've certainly haven't
    // visited this assignment)
    if (ps == da_map.end()) {
      DBG(cout << "not yet mapped - rotate." << endl;);
      return true;
    }
    AHASHSet& s = ps->second;
    // if hash is already there, assume that model has been visited
    if (s.find(hash) != s.end()) {
      DBG(cout << "assignment already visited - do not rotate." << endl;);
      return false;
    }
    // if hash not there, but there's too many assignments (i.e. we did too many
    // rotations through gids) models - do not rotate.
    if (rdepth && (s.size() > (rdepth - 1))) {
      DBG(cout << "assignment is new, but too many models - do not rotate." << endl;);
      return false;
    }
    // ok, allow to rotate
    DBG(cout << "assignment is new; currently " << s.size() << " visited - rotate." << endl;);
    return true;
  }

  // Returns a random integer from [0, limit]
  inline int random_int(int limit) { return (int)((limit+1.0)*rand()/(RAND_MAX+1.0)); }

  // Returns a random double from [0, 1)
  inline double random_double() { return ((double)rand()/(RAND_MAX+1.0)); }

} // anonymous namespace

/* Handles the RotateModel work item
 */
bool ExtendedModelRotator::process(RotateModel& rm)
{
  MUSData& md = rm.md();
  BasicGroupSet& gs = md.gset();
  OccsList& o_list = gs.occs_list();
  const GID& gid = rm.gid();
  const IntVector& orig_model = rm.model();
  unsigned rdepth = rm.rot_depth();
  unsigned rwidth = rm.rot_width();
  DBG(unsigned n_count = 0;);
  DBG(unsigned p_count = 0;);

#ifdef MULTI_THREADED
  throw logic_error("ExtendedModelRotator::process() is not ready for multi-threaded mode.");
#endif

  DBG(cout << "+ExtendedModelRotator::process(" << gid << "), (d,w) = " 
      << rdepth << "," << rwidth << endl;);

  // models are coded in terms of their deltas to the original model - a delta
  // is a list or set of variables in the original model that should be flipped;
  // a work queue entry consists of a group-set G (a set of group ids) and a 
  // and delta for it -- the invariant is that all of the groups in G are falsified
  // by the assignment represented by delta.
  queue<rot_queue_entry> rot_queue;

  // the first entry (delta is empty)
  rot_queue.push(rot_queue_entry(gid));
  // a copy of the original model -- will be used as working assignment.
  IntVector curr_ass(orig_model);
  AHASH curr_ass_hash = ass_hash(curr_ass);
  while (!rot_queue.empty()) {
    rot_queue_entry& e = rot_queue.front();
    GIDSet& gids = e._gids;
    DBG(cout << "Rotating gids=" << gids;);

    // update working assignment and its hash (don't forget to restore)
    DBG(cout << ", delta: ";);
    for (list<ULINT>::iterator pv = e._delta.begin(); pv != e._delta.end(); ++pv) {
      DBG(cout << *pv << " ";);
      flip(curr_ass, *pv);
      ass_hash_flip(curr_ass_hash, *pv);
    }
    DBG(cout << endl;);

    // see if we're allowed to proceed
    if (!allow_to_rotate(gids, curr_ass, curr_ass_hash, rdepth, rwidth)) {
      DBG(cout << "  not allowed to rotate." << endl;);
    } else {
      // ok, we're ready to roll:
      //   1. double-check that curr_model falsifies gid (in checked mode)
      //   2. for each modification to the model (currently just a single flip)
      //      calculate the set of falsified group ids, and throw the result
      //      onto the queue; if the resulting group-set has exactly one group
      //      we've got another necessary group;

      // make a list of all falsified clauses, and collect all of their variables
      // into a set -- this will be a set of candiates for flip
      IntSet cand_vars;
      BasicClauseVector f_clauses;
      f_clauses.reserve(1000);
      for (GIDSet::iterator pgid = gids.begin(); pgid != gids.end(); ++pgid) {
        const BasicClauseVector& gclauses = gs.gclauses(*pgid);	
        // collect the currently falsified clauses and their variables 
        DBG(cout << "  Group " << *pgid 
            << ((gclauses.size() < 100) ? " " : " UNSAT ") << "clauses:";);
        for (cvec_citerator pcl = gclauses.begin(); pcl != gclauses.end(); ++pcl) {
          if ((*pcl)->removed()) 
            continue;
          if (tv_clause(curr_ass, *pcl) == -1) {
            f_clauses.push_back(*pcl);
            for(CLiterator lpos = (*pcl)->abegin(); lpos != (*pcl)->aend(); ++lpos)
              cand_vars.insert(abs(*lpos));
            DBG(cout << " " << **pcl << "(UNSAT)";);
          }
          DBG(else if (gclauses.size() < 100) cout << " " << **pcl;);
        }
        DBG(cout << endl;);
      }
      DBG(cout << "  Total " << f_clauses.size() << " false clauses, " 
          << cand_vars.size() << " variables." << endl;);
      assert(!cand_vars.empty()); // must be non-empty, o/w gid is not UNSAT !

      // now, for each candidate -- flip and calculate the set of falsified group
      // ids -- this set will go into the queue
      for (IntSet::iterator pv = cand_vars.begin(); pv != cand_vars.end(); ++pv) {
        LINT lit = *pv * curr_ass[*pv]; // clauses with lit might become falsified
        flip(curr_ass, *pv);
        ass_hash_flip(curr_ass_hash, *pv);
        DBG(cout << "  Checking var " << *pv << ", assigned " << curr_ass[*pv] << ": ";);
        GIDSet new_gids; // these will be falsified gids
        // first, run through false clauses, and for any clause that is still false,
        // add it to new_gids;
        for (cvec_citerator pcl = f_clauses.begin(); pcl != f_clauses.end(); ++pcl)
          if (tv_clause(curr_ass, *pcl) == -1)
            new_gids.insert((*pcl)->get_grp_id());
        // now, run through the clauses that contain lit, and for any clause that
        // *became* falsified due to the flip, add its gid also to new_gids
        // early break: if we still have rwidth false gids, no point to check anymore 
        // since it will stay the same, or increase ...
        if (new_gids.size() <= rwidth) {
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
              // early break: if over rwidth, we're done ...
              if (new_gids.size() > rwidth)
                break;
            }
            ++pcl; // next clause
          }
        }     
        // ok, now we have the falsified group-set in new_gids; put them on the queue
        DBG(cout << "  Falsified gids: " << new_gids << endl;);
        // optimization: check before queuing whether it will be allowed to rotate -- 
        // if not - do not bother to queue
        DBG(cout << "  checking if they are worth to queue: ");
        if (allow_to_rotate(new_gids, curr_ass, curr_ass_hash, rdepth, rwidth)) {
          rot_queue_entry r(new_gids, e._delta);
          r._delta.push_back(*pv);
          rot_queue.push(r);
          DBG(cout << " put on the queue" << endl;);
        }
        // unflip
        flip(curr_ass, *pv);
        ass_hash_flip(curr_ass_hash, *pv);
      }
      // once a group-set is processed, update da_map and check if its a singleton - 
      // if yes, it should be remembered as necessary
      da_map[gids].insert(curr_ass_hash);
      DBG(cout << "Finished group-set " << gids << ", stored ass_hash"
          << " 0x" << hex << curr_ass_hash << dec << endl;);
      if (gids.size() == 1) {
        GID n_gid = *gids.begin();
        DBG(cout << "Group " << n_gid << " is necessary due to rotation " << flush;
            if (rm.nec_gids().find(n_gid) == rm.nec_gids().end())
              cout << "(new, count = " << ++n_count << ")";
            cout << endl;);
        rm.nec_gids().insert(n_gid);
      }
    } // if (allow_to_rotate ...)
    // restore working model
    for (list<ULINT>::iterator pv = e._delta.begin(); pv != e._delta.end(); ++pv) {
      flip(curr_ass, *pv);
      ass_hash_flip(curr_ass_hash, *pv);
    }
    // done this group-set
    rot_queue.pop();
    _num_points++;
    DBG(p_count++;);
  } // while (queue)
  DBG(cout << "Number of queue pops: " << p_count << ", da_map size = " << da_map.size() << endl;);
  rm.set_completed();
  if (rm.ignore_global()) { da_map.clear(); } // forget everything
  DBG(cout << "-ExtendedModelRotator::process(" << gid << ")" << endl;);
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

  // Checks whether a given assignment satisfies a set of clauses; return 1 for SAT, -1
  // for UNSAT, 0 for undetermined. A set is SAT iff all clauses are SAT, a set
  // is UNSAT iff at least one clause is UNSAT, undetermined otherwise
  int tv_group(IntVector& ass, const BasicClauseVector& clauses)
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

  // Calculates and returns the hash-value of a assignment
  AHASH ass_hash(const IntVector& ass)
  {
    ULINT hash = 0, mask = 0;
    const int bits = 8*sizeof(ULINT); // size of hash in bits: reduce if you want coarser hash
    for (size_t i = 0; i < ass.size(); i++) {
      if (i % bits == 0) {
        hash ^= mask;
        mask = 0;
      } else
        mask <<= 1;
      if (ass[i] == 1)
        mask |= 1;
    }
    hash ^= mask;
    return hash;
  }

  // Updates the hash-value to match a flip of the specified variable
  void ass_hash_flip(AHASH& hash, ULINT var)
  {
    const int bits = 8*sizeof(ULINT);
    // flip the corresponding bit in the hash
    ULINT mask = 1 << (bits - 1 - var % bits);
    hash ^= mask;
  }

  // Flips a variable in the assignment
  void flip(IntVector& ass, ULINT var) {
    LINT val = ass[var];
    assert(val != 0); // really shouldn't be flipping unassigned vars
    if (val)
      ass[var] = (val == 1) ? -1 : 1;
  }

} // anonymous namespace
