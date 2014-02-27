/*----------------------------------------------------------------------------*\
 * File:        bce_simplifier.cc
 *
 * Description: Implementation of BCE-based simplifier worker.
 *
 * Author:      antonb
 * 
 * Notes:
 *      1. only desctructive mode is supported for now.
 *
 *                                              Copyright (c) 2011, Anton Belov
\*----------------------------------------------------------------------------*/

#include <ext/hash_set>
#include <iostream>
#include <list>
#include "mtl/mheap.hh"           // minisat's heap
#include "basic_group_set.hh"
#include "bce_simplifier.hh"
#include "types.hh"

using namespace std;
using namespace __gnu_cxx;

//#define DBG(x) x

namespace {

  /** The "touched literals" queue; current implementation is using Minisat 
   * mutable heap. Note however that it would need to allocate 4*max_var ints
   * so I should also try boost.
   */
  class LitQueue {

    /* comparator for literals */
    struct LitOrder {
      // return true if l1 should be in-front of l2; we want the literal with 
      // smallest occs list of its opposite to be in the front (i.e. with the 
      // smallest key); among two literals with the same sized occs list, pick 
      // one with smallest occs list; break ties by integer comparison. Note 
      // that the comparator goes directly into map using the indexes.
      bool operator()(int i1, int i2) {
        int l1_s = _o_list.active_size_i(i1^1), l2_s = _o_list.active_size_i(i2^1); // negatives
        if (l1_s == l2_s) {
          l1_s = _o_list.active_size_i(i1); l2_s = _o_list.active_size_i(i2); // positives
          if (l1_s == l2_s) {
            l1_s = i1; l2_s = i2;
          }
        }
        assert(l1_s != l2_s);
        return (l1_s < l2_s);
      }
      LitOrder(const OccsList& o_list)  : _o_list(o_list) {}
      const OccsList& _o_list;
    };

    Minisat::Heap<LitOrder> _heap;      // *mutable* heap

  public:   

    LitQueue(const OccsList& o_list) : _heap(LitOrder(o_list)) {}

    bool empty(void) const { return _heap.empty(); }

    void insert(LINT lit) { _heap.insert(OccsList::l2i(lit)); }
    void insert_i(int i) { _heap.insert(i); } // direct version

    LINT removeMin(void) { return OccsList::i2l(_heap.removeMin()); }
    int removeMin_i(void) { return _heap.removeMin(); } // direct version

    // insert or percolate up/down
    void update(LINT lit) { _heap.update(OccsList::l2i(lit)); }
    void update_i(int i) { _heap.update(i); }
    
  };

  /* Returns true if the resolvent of the two given clauses is tautological */
  bool taut_resolvent(BasicClause *c1, BasicClause *c2, LINT lit);

} // anonymous namespace


/* Handles the SimplifyBCE work item
 */
bool BCESimplifier::process(SimplifyBCE& sb)
{
  // TEMP: supports only pre-processing for now ...
  if (!sb.destructive())
    throw logic_error("BCESimplifier::process() -- non-destructive simplification"
                      " is not yet supported");
  DBG(cout << "+BCESimplifier::process()" << endl;);
  MUSData& md = sb.md();

  // grab the write lock right away ...
  md.lock_for_writing(); 

  double t_start = RUSAGE::read_cpu_time();
  simplify(md.gset().occs_list(), &sb, nullptr);
  sb.cpu_time() = RUSAGE::read_cpu_time() - t_start;

  md.release_lock();
  sb.set_completed();
  DBG(cout << "-BCESimplifier::process()" << endl;);
  return sb.completed();
}

/** This is for external callers: runs BCE on the specified clause-set. The
 * eliminated clauses will be removed from the clause set. Note that it will
 * create its own OccsList for this.
 */
void BCESimplifier::simplify(BasicClauseSet& clset)
{
  OccsList occs;
  occs.resize(clset.get_max_var() + 1);
  for (BasicClause* cl : clset) {
    for (LINT lit : *cl) {
      occs.clauses(lit).push_back(cl);
      ++(occs.active_size(lit));
    }
  }
  simplify(occs, nullptr, &clset);
  // done
}


/* This is the actual elimination logic; works on the OccsList; if psb != nullptr,
 * updates it accordingly; if pclset != nullptr detaches clauses from it
 */
void BCESimplifier::simplify(OccsList& o_list, SimplifyBCE* psb, BasicClauseSet* pclset)
{
  // extra control params
  bool move2g0 = (psb != nullptr) && psb->blocked_2g0(); // move to g0 instead of removing
  bool ig0 = (psb != nullptr) && psb->ignore_g0(); // ignore g0 clauses

  // vector of removed clauses, used only with move2g0
  BasicClauseVector r_cls;

  // "touched" queue
  LitQueue t_queue(o_list);

  // touch all literals
  int lit_i = 0;
  for (OccsList::as_citer ps = o_list.as_begin(); ps != o_list.as_end(); ++ps, ++lit_i)
    if (*ps)
      t_queue.insert_i(lit_i);

  // off we go ...
  while(!t_queue.empty()) {
    int lit_i = t_queue.removeMin_i();
    LINT lit = OccsList::i2l(lit_i);
    BasicClauseList& cands = o_list.clauses_i(lit_i);
    NDBG(cout << "Checking literal " << lit
        << "(-occ=" << o_list.active_size_i(lit_i^1)
        << ",+occ=" << o_list.active_size_i(lit_i) << ")" << endl;);
    for(BasicClauseList::iterator pcand = cands.begin(); pcand != cands.end(); ) {
      BasicClause* cand =  *pcand; // candidate clause
      NDBG(cout << "  candidate clause: "; cand->dump(); cout << endl;);
      if (cand->removed()) {
        // known to be removed (lazily); remove from map, unless move2g0 is true,
        // and the clause is in g0
        if (move2g0 && cand->get_grp_id() == 0)
          ++pcand;
        else
          pcand = cands.erase(pcand);
        NDBG(cout << "  removed earlier." << endl;);
        continue;
      }
      if (ig0 && cand->get_grp_id() == 0) {
        NDBG(cout << "  candidate clause is in g0, ignoring" << endl;);
        ++pcand;
        continue;
      }
      // inner loop -- look for non-taut resolvent among clashing clauses
      bool found = false;
      BasicClauseList& clashes = o_list.clauses_i(lit_i^1);
      for (BasicClauseList::iterator pclash = clashes.begin();
           pclash != clashes.end(); ) {
        BasicClause* clash = *pclash;
        NDBG(cout << "      clash: "; clash->dump(); cout << endl;);
        if (clash->removed()) {
          // known to be removed (lazily); remove from map, unless move2g0 is
          // true, and the clause is in g0
          if (move2g0 && cand->get_grp_id() == 0)
            ++pclash;
          else
            pclash = clashes.erase(pclash);
          NDBG(cout << "      removed earlier." << endl;);
          continue;
        }
        if (ig0 && clash->get_grp_id() == 0) {
          NDBG(cout << "      clash is in g0, ignoring" << endl;);
          ++pclash;
          continue;
        }
        if (!taut_resolvent(cand, clash, lit)) {
          NDBG(cout << "      non-tautological; aborting." << endl;);
          found = true;
          break;
        }
        NDBG(cout << "      tautological; continuing checking." << endl;);
        ++pclash;
      }
      if (found) {
        NDBG(cout << "    non-tautological resolvent found, clause is not blocked."
            << endl;);
        ++pcand;
      } else {
        NDBG(cout << "  all resolvents are tautological, clause is BLOCKED."
            << endl;);
        // note that we're not going to clean up the map completely - just mark
        // the clause removed, remove it from cands list, and remove it from the
        // group-set; later on marked clauses will be dropped from the map
        // automatically
        cand->mark_removed();
        if (move2g0) {
          r_cls.push_back(cand); // to "unremove" it later
          ++pcand;
        } else
          pcand = cands.erase(pcand);
        GID cand_gid = cand->get_grp_id();
        if (psb != nullptr) {
          if (!move2g0 || (move2g0 && cand_gid != 0)) {
            if (--(psb->md().gset().a_count(cand_gid)) == 0) { // group is gone
              psb->md().r_gids().insert(cand_gid);
              psb->md().r_list().push_front(cand_gid);
              ++psb->rg_count();
            }
            if (move2g0) {
              assert(cand_gid != 0);
              cand->set_grp_id(gid_Undef); // fooling gset
              psb->md().gset().set_cl_grp_id(cand, 0);
              // note, that if we're in -pc mode (i.e. cand->get_slit() != 0),
              // then the asserting unit clause will have to be added to g0 --
              // this is done outside the main loop to avoid messing up occlists
            }
          }
        }
        if (pclset != nullptr) { pclset->detach_clause(cand); }
        // touch all -literals of the clause
        for (Literator pl = cand->abegin(); pl != cand->aend(); ++pl) {
          --(o_list.active_size(*pl)); // update size -- very important for performance !
          t_queue.update(-*pl);
        }
        if (psb != nullptr) { ++psb->rcl_count(); }
      }
    }
  }
  // if move2g0 is specified, "unremove" the removed clauses
  if (move2g0) {
    DBG(cout << "Unremoving blocked clauses" << endl;);
    vector<LINT> unit(1);
    for (BasicClause* cl : r_cls) {
      assert(cl->get_grp_id() == 0 && "removed clauses should end up in g0");
      assert(cl->removed() && "clause should be removed");
      cl->unmark_removed();
      DBG(cout << "  unremoved "; cl->dump(); cout << endl;);
      if (cl->get_slit() != 0) {
        unit[0] = -cl->get_slit();
        cl->set_slit(0);
        BasicClause* ucl = psb->md().gset().create_clause(unit);
        psb->md().gset().set_cl_grp_id(ucl, 0);
      }
    }
  }
}


//
// ------------------------  Local implementations  ----------------------------
//

namespace {

  // returns true if the resolvent of the two given clauses is tautological; 
  // this does the full (long) check; relies on the fact the active literals 
  // in clauses are sorted in increasing order of their absolute values --
  // if not, then sorts them
  bool taut_resolvent(BasicClause *c1, BasicClause *c2, LINT lit)
  {
    if (c1->unsorted())
      c1->sort_alits();
    if (c2->unsorted())
      c2->sort_alits();
    // merge, implicitly, two sorted regions, look for early termination
    bool res = false; // true when tautological
    Literator pl1 = c1->abegin(), pl2 = c2->abegin();
    while (pl1 != c1->aend() && pl2 != c2->aend()) {
      ULINT v1 = abs(*pl1), v2 = abs(*pl2);
      if (v1 < v2)
        ++pl1;
      else if (v1 > v2)
        ++pl2;
      else if (v1 == (ULINT)abs(lit)) { // v1 == v2 == abs(lit)
        ++pl1; ++pl2;
      } else { // v1 == v2
        bool neg1 = *pl1 < 0, neg2 = *pl2 < 0;
        if ((neg1 ^ neg2) == 0) { // same sign
          ++pl1; ++pl2;
        } else { // clashing, but not lit -- tautology
          res = true;
          break;
        }
      }
    }
    return res;
  }

} // anonymous namespace
