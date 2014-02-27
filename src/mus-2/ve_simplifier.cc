/*----------------------------------------------------------------------------*\
 * File:        ve_simplifier.cc
 *
 * Description: Implementation of VE-based simplifier worker.
 *
 * Author:      antonb
 * 
 * Notes:
 *      1. The algorithm implemented here is very close to SatElite. The key 
 *      point is to do variable elimination and subsumption and self-subsumption
 *      together. Note that this combo supercedes BCP.
 *
 *                                              Copyright (c) 2011, Anton Belov
\*----------------------------------------------------------------------------*/

#ifdef NDEBUG
#undef NDEBUG // enable assertions (careful !)
#endif

#include <cassert>
#include <deque>
#include <iostream>
#include <queue>
#include <vector>
#include "mtl/mheap.hh"         // minisat's heap
#include "mtl/queue.hh"         // minisat's queue
#include "basic_group_set.hh"
#include "ve_simplifier.hh"
#include "types.hh"

using namespace std;
using namespace __gnu_cxx;

//#define DBG(x) x

namespace {

  /* A queue of non-empty clauses
   */
  class ClauseQueue {
  public:       
    /* Insert clause */
    void insert(BasicClause* cl) { 
      assert(cl->asize() > 0);
      //if (cl->asize() == 1) _q.push_front(cl); else 
      _q.push_back(cl);
    }
    /* Get the front */
    BasicClause* front(void) { return _q.front(); }
    /* Pop the front */
    void pop(void) { return _q.pop_front(); }
    /* Size */
    size_t size(void) { return _q.size(); }
    /* Empty */
    bool empty() { return _q.size() == 0; }
    /* Returns true if the queue has at least one unit */
    bool has_units(void) { return !_q.empty() && _q.front()->asize() == 1; }
  private:      
    deque<BasicClause*> _q;
  };

  /** The "touched variable" heap; current implementation is using Minisat 
   * mutable heap. Note however that it would need to allocate 4*max_var ints
   * so I should also try boost.
   */
  class VarHeap {

    /* comparator for variables */
    struct VarOrder {
      // return true if v1 should be in-front of v2; we want the variabled with
      // the smallest cost (sum of positive and negative occurences)
      bool operator()(ULINT v1, ULINT v2) {
        unsigned c1 = _o_list.active_size(v1) * _o_list.active_size(-v1);
        unsigned c2 = _o_list.active_size(v2) * _o_list.active_size(-v2);
        return c1 < c2;
      }
      VarOrder(const OccsList& o_list)  : _o_list(o_list) {}
      const OccsList& _o_list;
    };

    Minisat::Heap<VarOrder> _heap;      // *mutable* heap

  public:   

    VarHeap(const OccsList& o_list) : _heap(VarOrder(o_list)) {}

    bool empty(void) const { return _heap.empty(); }

    void insert(ULINT v) { _heap.insert(v); }

    ULINT removeMin(void) { return _heap.removeMin(); }

    // insert or percolate up/down
    void update(ULINT v) { _heap.update(v); }
    
  };

  // see implementation's comment for detailed spec

  /* Returns true if c1 subsumes c2. */
  bool subsumes(const BasicClause* c1, const BasicClause* c2);
  /* Helper to remove a clause from group-set -- updates MUSData */
  void remove_clause(SimplifyVE& sv, BasicClause* c);
  /* Removes all clauses subsumed by c from gs. */
  void remove_subsumed(SimplifyVE& sv, const BasicClause* c);
  /* Resolves c1 and c2 on v, returns resolvent or NULL in case of tautology. */
  BasicClause* resolve(SimplifyVE& sv, const BasicClause* c1, 
                       const BasicClause *c2, ULINT v);
  /* Removes all clauses that have literal l */
  void remove_all(SimplifyVE& sv, LINT l);
  /* Removes all clauses that have variable v */
  void remove_all(SimplifyVE& sv, ULINT v);
  /* Propagates all unit clauses in the queue */
  bool bcp(SimplifyVE& sv, ClauseQueue& cqueue);
  /* Performs variable elimination of variable v */
  bool eliminate_var(SimplifyVE& sv, ULINT v, VarHeap& vheap, ClauseQueue& cqueue);
  /* Performs self-subsumption on clause c */
  void self_subsume(SimplifyVE& sv, const BasicClause* c, ClauseQueue& cqueue);

  // stats (TEMP)
  int r_clauses = 0;  // removed clauses
  int a_clauses = 0;  // added clauses
  int r_vars = 0;     // removed variables
  int r_groups = 0;   // removed gruops

} // anonymous namespace


/* Handles the SimplifyVE work item. This includes variable elimination, 
 * subsumtion, and self-subsumption.
 */
bool VESimplifier::process(SimplifyVE& sv)
{
  //bool group_mode = sv.group_mode();
  assert(!sv.group_mode());
  DBG(cout << "+VESimplifier::process()" << endl;);
  MUSData& md = sv.md();
  BasicGroupSet& gs = md.gset();
  OccsList& occs = gs.occs_list();
  ClauseQueue cqueue;  // clause queue
  VarHeap vheap(occs); // variable heap

  double t_start = RUSAGE::read_cpu_time();

  // grab the write lock right away ...
  md.lock_for_writing(); 

  // initialize with units
  for (BasicClauseVector::const_iterator pcl = gs.units().begin(); 
       pcl != gs.units().end(); ++pcl)
    if (!(*pcl)->removed())
      cqueue.insert(*pcl);

  // remember the maximum ID of the original clauses
  ULINT max_orig_id = gs.max_id();

  while (1) {
    int last_diff = r_clauses - a_clauses;      // accounting
    cout << "c VE: new iteration, start=" << (r_clauses - a_clauses);

    // BCP
    bcp(sv, cqueue);
    if (sv.conflict()) {
      cout << " top-level conflict !" << endl;
      break;
    }
    cout << " bcp1=" << (r_clauses - a_clauses) << flush;
    assert(cqueue.empty());

#if 0
    // self-subsumption -- cannot be used safely at the moment
    for (BasicClauseVector::const_iterator pcl = gs.begin(); pcl != gs.end(); ++pcl)
      if (!(*pcl)->removed())
        cqueue.insert(*pcl);
    while (!cqueue.empty()) {
      BasicClause* cl = cqueue.front();
      if (cl->asize() == 1) // got to the units -- get out to do BCP
        break;
      if (!cl->removed())
        self_subsume(sv, cl, cqueue);
      cqueue.pop();
    }
    cout << " ssr=" << (r_clauses - a_clauses) << flush;
#endif

    // another BCP
    bcp(sv, cqueue);
    if (sv.conflict()) {
      cout << " top-level conflict !" << endl;
      break;
    }
    cout << " bcp2=" << (r_clauses - a_clauses) << flush;
    assert(cqueue.empty());

    // subsumption -- check if any of the *original* clauses subsumes anything
    for (BasicClauseVector::const_iterator pcl = gs.begin(); pcl != gs.end(); ++pcl)
      if (!(*pcl)->removed() && (*pcl)->get_id() <= max_orig_id)
        remove_subsumed(sv, *pcl);
    cout << " sub=" << (r_clauses - a_clauses) << flush;

    // elimination -- check active variables (maybe: limit size)
    for (ULINT var = 1; var <= gs.max_var(); var++)
      if (occs.active_size(var) || occs.active_size(-var))
        vheap.insert(var);
    while(!vheap.empty()) {
      ULINT var = vheap.removeMin();
      NDBG(cout << "Eliminating variable " << var << endl;);
      eliminate_var(sv, var, vheap, cqueue);
      if (sv.conflict()) {
        cout << " top-level conflict !" << endl;
        goto _done;
      }
      //if (r_vars > 2) goto __done; // TEMP
    }
    cout << " ve=" << (r_clauses - a_clauses) << flush;

    int diff = r_clauses - a_clauses;
    cout << " total removed clauses: " << r_clauses << ", "
         << "added clauses: " << a_clauses << ", "
         << "net removed: " << diff << endl;
    if ((diff - last_diff) <= 0.1*last_diff) {
      cout << "c VE: done." << endl;
      break;
    }
  }
  // done main loop
 _done:
  // check if there was conflict -- if yes, then all clauses except the conflict
  // clause have to be removed
  if (sv.conflict()) {
    for (cvec_iterator pcl = gs.begin(); pcl != gs.end(); ++pcl)
      if (*pcl != sv.conflict_clause())
        if (!(*pcl)->removed())
          remove_clause(sv, *pcl);
  }
  md.release_lock();
  sv.cpu_time() = RUSAGE::read_cpu_time() - t_start;    
  sv.rcl_count() = r_clauses - a_clauses;
  sv.set_completed();
  DBG(cout << "-VESimplifier::process()." << endl;);
  return sv.completed();
}

/* Reconstructs the solution (inside sv.md()) in terms of the original
 * clauses. 'sv' is expected to be the instance used during the last 
 * call to process()
 * NOTE: this is a prototype/experimental version
 */
void VESimplifier::reconstruct_solution(SimplifyVE& sv)
{
  DBG(cout << "+VESimplifier::reconstruct_solution()." << endl;);
  assert(!sv.group_mode()); // TEMP
  MUSData& md = sv.md();
  BasicGroupSet& gs = md.gset();
  vector<ULINT>& trace = sv.trace();
  const SimplifyVE::DerivData& dd = sv.dd();

  double t_start = RUSAGE::read_cpu_time();

  // this will keep the reconstructed levels -- current, and next 
  // (in the opposite direction)
  HashedClauseSet curr_lvl;
  vector<BasicClause*> to_remove, to_insert;
  
  // make the initial level of clauses -- in case of conflict this is just
  // the empty clause, otherwise these are the clauses in the computed MUS
  if (sv.conflict()) {
    md.make_empty_gmus();
    curr_lvl.insert(sv.conflict_clause());
  } else {
    for (GIDSetCIterator pgid = md.nec_gids().begin(); 
         pgid != md.nec_gids().end(); ++pgid) {
      assert(gs.gclauses(*pgid).size() == 1);
      curr_lvl.insert(*gs.gclauses(*pgid).begin());
    }
  }

  for (vector<ULINT>::reverse_iterator pv = trace.rbegin(); pv != trace.rend(); ++pv) {
    ULINT var = *pv;
    NDBG(cout << "Current level: ";
         for (cset_iterator pcl = curr_lvl.begin(); pcl != curr_lvl.end(); ++pcl)
           (*pcl)->dump();
         cout << endl;);
    DBG(cout << "processing " << var << ", level size = " << curr_lvl.size() 
        << endl;);

    // run through the current level -- any clause that is not involved with var
    // is kept; for the clauses involved with var, their premises are copied instead
    to_remove.clear();
    to_insert.clear();
    bool single_res = true;
    for (cset_iterator pcl = curr_lvl.begin(); pcl != curr_lvl.end(); ++pcl) {
      BasicClause* cl = *pcl;
      SimplifyVE::DerivData::const_iterator pdd = dd.find(cl);
      if ((pdd != dd.end()) && (pdd->second.v == var)) {
          to_remove.push_back(*pcl);
          to_insert.push_back(pdd->second.r1);
          to_insert.push_back(pdd->second.r2);
          DBG(cout << "    replacing " << **pcl << " with " 
              << *(pdd->second.r1) << " and " << *(pdd->second.r2) << endl;);
          if (pdd->second.count > 1) // assume unsafe
            single_res = false;
      }
    }
    // +TEMP test for safety: compute VE of to_insert, and for each clause, test
    // if it is in curr_lvl -- if not, then the reconstruction might be unsafe
    vector<BasicClause*> pos, neg;
    for (vector<BasicClause*>::iterator pcl = to_insert.begin(); 
         pcl != to_insert.end(); ++pcl) {
      CLiterator plit = (*pcl)->afind(var);
      assert(plit != (*pcl)->aend());
      ((*plit > 0) ? pos : neg).push_back(*pcl);
    }
    vector<BasicClause*> ve;
    for (vector<BasicClause*>::iterator pp = pos.begin(); pp != pos.end(); ++pp)
      for (vector<BasicClause*>::iterator pn = neg.begin(); pn != neg.end(); ++pn) {
        BasicClause* res = resolve(sv, *pp, *pn, var);
        if (res != NULL)
          ve.push_back(res);
      }
    NDBG(cout << "    calculated resolvents: ";);
    bool safe = true;
    for (vector<BasicClause*>::iterator pcl = ve.begin(); pcl != ve.end(); ++pcl) {
      NDBG(cout << **pcl;);
      // now we need to check if the clause is in the current level: lookup the
      // clause in the group-set; if there, check if its in the current level
      BasicClause* cand = gs.lookup_clause(*pcl);
      if (cand == NULL) {
        NDBG(cout << "(u-new) ";);
        safe = false; break;
      } else if (curr_lvl.find(cand) == curr_lvl.end()) {
        NDBG(cout << "(u-notin) ";);
        safe = false; break;
      } else {
        NDBG(cout << "(s) ";);
      }
    }
    if (!safe)
      _unsound = true;
    if (!single_res)
      _unsound_mr = true;
    NDBG(cout << endl << "    the reconstruction is " 
        << ((safe && single_res) ? "safe" : "NOT safe") << endl;);
    if (!(safe && single_res)) {
      cout << "c  unsafe reconstruction step: " 
           <<  ((safe) ? "" : "violated-equality ")
           <<  ((single_res) ? "" : "multi-resolvent ")
           << endl;
    }
    // -TEMP
    for (vector<BasicClause*>::iterator pcl = to_remove.begin(); 
         pcl != to_remove.end(); ++pcl) {
      BasicClause* cl = *pcl;
      cl->mark_removed();
      md.nec_gids().erase(cl->get_grp_id());
      md.r_gids().insert(cl->get_grp_id());
      curr_lvl.erase(cl);
    }
    for (vector<BasicClause*>::iterator pcl = to_insert.begin(); 
         pcl != to_insert.end(); ++pcl) {
      BasicClause* cl = *pcl;
      cl->unmark_removed();
      md.nec_gids().insert(cl->get_grp_id());
      md.r_gids().erase(cl->get_grp_id());
      curr_lvl.insert(cl);
    }
  }
  sv.cpu_time() = RUSAGE::read_cpu_time() - t_start;    
  DBG(cout << "-VESimplifier::reconstruct_solution()." << endl;);
}

//
// ------------------------  Local implementations  ----------------------------
//

namespace {

  /* Returns true if c1 subsumes c2.
   * @pre sorted(c1), sorted(c2)      // optimization
   * @post none
   * @return (c1 \subset c2)
   */
  bool subsumes(const BasicClause* c1, const BasicClause* c2)
  {
    assert(!(c1->unsorted() || c2->unsorted()));
    if (c1->asize() >= c2->asize())
      return false;
    // fast check: 1 in abstraction of c1, 0 in absraction of c2 on the same place
    if (c1->abstr() & ~c2->abstr())
      return false;
    // slow check
    register LINT* first = &(const_cast<BasicClause*>(c1)->lits()[0]); 
    register LINT* first_end = first + c1->asize(); 
    register LINT* second = &(const_cast<BasicClause*>(c2)->lits()[0]); 
    register LINT* second_end = second + c2->asize(); 
    while (first != first_end) {
      // find 'first' in the second clause
      while (*second != *first) {
        ++second; 
        if (second == second_end) { return false; }
      }
      ++first;
    }
    return true;
  }

  /* Helper to remove a clause from group-set -- updates MUSData
   * @pre c \in gs
   * @post c \notin gs'
   */
  void remove_clause(SimplifyVE& sv, BasicClause* c)
  {
    MUSData& md = sv.md();
    BasicGroupSet& gs = md.gset();
    assert(!c->removed());
    gs.remove_clause(c);
    ++r_clauses;
    GID gid = c->get_grp_id();
    if (gs.a_count(gid) == 0) {
      md.r_gids().insert(gid);
      md.r_list().push_front(gid);      
      r_groups++;
    }
  }

  /* Makes a list of all clauses in gs that are subsumed by clause c. The
   * clauses are appended to the end of the 'sub' vector.
   * @pre c \in gs
   * @post c' \in gs & subsumes(c, c') -> c' \in sub
   */
  void calculate_subsumed(SimplifyVE& sv, const BasicClause* c, 
                          BasicClauseVector& sub) 
  {
    assert(!c->removed() && c->asize() > 0);
    NDBG(cout << "+calculate_subsumed(): computing clauses subsumed by "; c->dump(); 
        cout << endl;);
    BasicGroupSet& gs = sv.md().gset();
    // special case: c is the empty clause, everything is subsumed by it
    if (c->asize() == 0) {
      for (BasicClauseVector::iterator pcl = gs.begin(); pcl != gs.end(); ++pcl)
        if (!(*pcl)->removed() && (*pcl)->asize())
          sub.push_back(*pcl);
      return;
    }
    // non-empty here only
    OccsList& occs = gs.occs_list();
    // find literal with the shortest occs list
    CLiterator pmin_l = c->abegin();
    for (CLiterator pl = pmin_l+1; pl != c->aend(); ++pl)
      if (occs.active_size(*pl) < occs.active_size(*pmin_l))
        pmin_l = pl;
    BasicClauseList& clauses = occs.clauses(*pmin_l);
    for (BasicClauseList::iterator pcl = clauses.begin(); pcl != clauses.end(); ) {
      NDBG(cout << "  checking "; (*pcl)->dump(););
      if ((*pcl)->removed()) { // already removed (lazily), remove from list
        NDBG(cout << " already removed." << endl;);
        pcl = clauses.erase(pcl);
        continue;
      }
      if (sv.sub_lim() >= 0 && (*pcl)->asize() > (unsigned)sv.sub_lim()) {
        NDBG(cout << " aborted, clause is too long." << endl;);
      } else if (subsumes(c, *pcl)) { // subsumed - add to the list
        NDBG(cout << " subsumed, adding to the list.";);
        sub.push_back(*pcl);
      }
      NDBG(cout << endl;);
      ++pcl;
    }       
    // done    
    NDBG(cout << "-calculate_subsumed(): done." << endl;);
  }

  /* Removes all clauses subsumed by c from gs. TODO: use calculate_subsumed()
   * @pre c \in gs
   * @post c' \in gs\gs' -> (c' != c) & subsumes(c, c')
   */
  void remove_subsumed(SimplifyVE& sv, const BasicClause* c) 
  {
    assert(!c->removed() && c->asize() > 0);
    NDBG(cout << "+remove_subsumed(): removing clauses subsumed by "; c->dump(); 
        cout << endl;);
    BasicClauseVector subs;
    calculate_subsumed(sv, c, subs);
    for (BasicClauseVector::iterator pcl = subs.begin(); pcl != subs.end(); ++pcl)
      remove_clause(sv, *pcl);
    // done    
    NDBG(cout << "-remove_subsumed(): done, removed " << subs.size() 
        << " clauses." << endl;);
  }

  /* Resolves c1 and c2 on v, returns resolvent or NULL in case of tautology.
   * @pre (v \in clash(c1, c2)), sorted(c1), sorted(c2)
   *      plus no dublicates, not tautology -- assumed throught BOLT
   * @return if (clash(c1,c2) = {v}) then c1 R_v c2 else NULL
   * @post if rv != NULL, then rv = c1 R_v c2 & sorted(rv) & !dulicates(rv)
   */
  BasicClause* resolve(SimplifyVE& sv, const BasicClause* c1, 
                       const BasicClause *c2, ULINT v)
  {
    BasicGroupSet& gs = sv.md().gset();
    assert(!c1->unsorted() && !c2->unsorted());
    assert(c1->afind(v) != c1->aend());
    assert(c2->afind(v) != c2->aend());
    assert(*c1->afind(v) + *c2->afind(v) == 0); // opposite signs
    NDBG(cout << "=resolve(): resolving "; c1->dump(); cout << " with ";
        c2->dump(); cout << ": ");
    vector<LINT> res;
    res.reserve(c1->asize() + c2->asize() - 2);
    CLiterator pl1 = c1->begin(), pl2 = c2->begin();
    while (pl1 != c1->aend() && pl2 != c2->aend()) {
      ULINT v1 = abs(*pl1), v2 = abs(*pl2);
      if (v1 < v2) {
        res.push_back(*pl1);
        ++pl1;
      } else if (v1 > v2) {
        res.push_back(*pl2);
        ++pl2;
      } else if (*pl1 + *pl2 == 0) { // opposite signs
        if (v1 != v) { // resolvent tautological
          NDBG(cout << " tautology" << endl;);
          return NULL;
        }
        ++pl1; ++pl2;
      } else { // same signs
        res.push_back(*pl1);
        ++pl1; ++pl2;
      }
    }
    // copy the rest ...
    for ( ; pl1 != c1->aend(); ++pl1)
      res.push_back(*pl1);
    for ( ; pl2 != c2->aend(); ++pl2)
      res.push_back(*pl2);
    // if we got here, then res has the literals of the resolvent
    BasicClause *r = gs.make_clause(res);
    NDBG(r->dump(); cout << endl;);
    return r;
  }

  /* Removes all clauses that have literal l
   * @pre none
   * @post c \in gs' -> v \notin var(c)
   */
  void remove_all(SimplifyVE& sv, LINT l)
  {
    BasicGroupSet& gs = sv.md().gset();
    BasicClauseList& cp = gs.occs_list().clauses(l);
    for (BasicClauseList::iterator pcl = cp.begin(); pcl != cp.end(); ) {
      if (!(*pcl)->removed())
        remove_clause(sv, *pcl);
      pcl = cp.erase(pcl);
    }
    assert(gs.occs_list().active_size(l) == 0);
  }
           
  /* Removes all clauses that have variable v
   * @pre none
   * @post c \in gs' -> v \notin var(c)
   */
  void remove_all(SimplifyVE& sv, ULINT v)
  {
    remove_all(sv, (LINT)v);
    remove_all(sv, -(LINT)v);
  }

  /* Does unit propagation of a unit clauses in cqueue
   */
  bool bcp(SimplifyVE& sv, ClauseQueue& cqueue)
  {
    NDBG(cout << "=bcp(): propagating unit clauses." << endl;);
    BasicGroupSet& gs = sv.md().gset();
    OccsList& occs = gs.occs_list();
    SimplifyVE::DerivData& dd = sv.dd();
    while (cqueue.has_units()) {
      BasicClause* uc = cqueue.front();
      assert(uc->asize() == 1);
      NDBG(cout << "  got " << *uc << ": ");
      if (uc->removed()) {
        NDBG(cout << "already processed, skipping." << endl;);
      } else {
        LINT lit = *uc->abegin();
        remove_all(sv, lit);
        NDBG(cout << "removed satisfied, " << endl;);
        BasicClauseList& cls = occs.clauses(-lit);
        for (BasicClauseList::iterator pcl = cls.begin(); pcl != cls.end(); ) {
          if ((*pcl)->removed()) { // already removed (lazily), tidy up the list
            pcl = cls.erase(pcl);
            continue;
          }
          BasicClause* cl = *pcl;
          NDBG(cout << "    propagating through " << *cl << ":" << flush;);
          vector<LINT> lits;
          for (CLiterator plit = cl->abegin(); plit != cl->aend(); ++plit)
            if (*plit != -lit)
              lits.push_back(*plit);
          BasicClause* res = gs.make_clause(lits);
          NDBG(cout << " got " << *res << flush;);
          // check if the clause is already in the set -- if not, add it and its 
          // derivation data; otherwise ignore  
          BasicClause* old_res = gs.lookup_clause(res);
          if (old_res == NULL) {
            assert(!sv.group_mode());
            gs.add_clause(res);
            gs.set_cl_grp_id(res, res->get_id());
            dd.insert(make_pair(res, SimplifyVE::ResData(uc, cl, abs(lit), 1)));
            ++a_clauses;
            NDBG(cout << " new, added; " << flush;);
            // now, special cases: empty clause and unit clause
            if (res->asize() == 0) {
              // empty clause 
              NDBG(cout << " conflict, terminating." << endl;);
              sv.set_conflict_clause(res);
              sv.trace().push_back(abs(lit));
              return false;
            }
            if (res->asize() == 1) {
              // unit clause -- queue
              NDBG(cout << " unit, queueued; ";);
              cqueue.insert(res);
            }
          }
          // drop the clause
          remove_clause(sv, cl);
          pcl = cls.erase(pcl);
          NDBG(cout << " removed strengthened." << endl;);
        }
        assert(gs.occs_list().active_size(lit) == 0);
        assert(gs.occs_list().active_size(-lit) == 0);
        sv.trace().push_back(abs(lit));
      }
      cqueue.pop();
    }
    return true;
  }

  /* Attempts to perform variable elimination of variable v; returns: true
   * if variables is eliminated (this includes conflict), false if not
   *
   * @pre none
   * @body {
   *    foreach c1 \in occs(gs, v) {
   *      foreach c2 \in occs(gs, -v) {
   *        rs = resolve_and_add(gs, c1, c2)
   *        remove_subsumed(rs)
   *      }
   *    }
   *    remove_all(gs, v)
   * }
   * @post if rv, then {
   *       (occs(gs, v) = \emptyset) & (occs(gs, -v) = \emptyset) &
   *       (c \in gs' <-> ((c \in gs) & (v \notin var(c))) | 
   *                      ((c \notin gs) & \exists c1, c2 \in gs \ gs' (c = c1 R_v c2))
   *       }
   */
  bool eliminate_var(SimplifyVE& sv, ULINT v, VarHeap& vheap, ClauseQueue& cqueue)
  {
    NDBG(cout << "+eliminate_var(): elimitating " << v << endl;);
    BasicGroupSet& gs = sv.md().gset();
    OccsList& occs = gs.occs_list();
    if (!occs.active_size(v) && !occs.active_size(-v))
      return true;
    assert(occs.active_size(v) || occs.active_size(-v));
    // check for pure 
    if (!(occs.active_size(v) && occs.active_size(-v))) { 
      LINT pure_lit = occs.active_size(v) ? (LINT)v : -(LINT)v;
      // remove and put all variables on the queue
      BasicClauseList& cls = occs.clauses(pure_lit);
      for (BasicClauseList::iterator pcl = cls.begin(); pcl != cls.end(); ) {
        if (!(*pcl)->removed()) {
          for (CLiterator plit = (*pcl)->abegin(); plit != (*pcl)->aend(); ++plit)
            if (*plit != pure_lit)
              vheap.update(abs(*plit));
          remove_clause(sv, *pcl);
        }
        pcl = cls.erase(pcl);
      }
      ++r_vars;
      return true;
    }
    // ok, not pure, do the work ...
    BasicClauseList& cls1 = occs.clauses(v);
    BasicClauseList& cls2 = occs.clauses(-v);
    SimplifyVE::DerivData& dd = sv.dd();
    int gain = occs.active_size(v) + occs.active_size(-v);
    SimplifyVE::DerivData local_dd; // potential resolvents
    for (BasicClauseList::iterator pcl1 = cls1.begin(); pcl1 != cls1.end(); ) {
      if ((*pcl1)->removed()) { // already removed (lazily), tidy up the list
        pcl1 = cls1.erase(pcl1);
        continue;
      }
      for (BasicClauseList::iterator pcl2 = cls2.begin(); pcl2 != cls2.end(); ) {
        if ((*pcl2)->removed()) { // already removed (lazily), tidy up the list
          pcl2 = cls2.erase(pcl2);
          continue;
        }
        // ok, got the clauses, try them ...
        BasicClause *cl1 = (*pcl1), *cl2 = (*pcl2), *res = resolve(sv, cl1, cl2, v);
        NDBG(cout << "  trying " << *cl1 << " with " << *cl2 << " got ";
            if (res == NULL) 
              cout << "tautology" << endl; 
            else if (sv.res_lim() >= 0 && res->asize() > (unsigned)sv.res_lim())
              cout << "too long" << endl;
            else
              cout << *res << flush;);
        if (res != NULL) {
          // check limit
          if (sv.res_lim() >= 0 && res->asize() > (unsigned)sv.res_lim()) {
            NDBG(cout << "-eliminate_var(): resolvent is too long, aborted." << endl;);
            return false;
          }
          // do a check against group-set -- if not there, remember the clause
          if (gs.exists_clause(res)) {
            NDBG(cout << " already in the set; skipping" << endl;);
          } else {
            // check for units and for conflicts here (during the "trial"); the point
            // is that even if elimination aborts later, the derived units (and empty
            // clause *are* implied, and so we can add them right away
            if ((res->asize() <= 1) && !gs.exists_clause(res)) {
              assert(!sv.group_mode());
              gs.add_clause(res);
              gs.set_cl_grp_id(res, res->get_id());
              dd.insert(make_pair(res, SimplifyVE::ResData(cl1, cl2, v, 1)));
              ++a_clauses;
              NDBG(cout << " new unit or empty, added; " << flush;);
              if (res->asize() == 0) {
                // empty clause 
                NDBG(cout << " conflict, terminating." << endl;);
                sv.set_conflict_clause(res);
                sv.trace().push_back(v);
                return true;
              }
              // unit - enqueue
              cqueue.insert(res);
            } else {
              // insert locally
              local_dd.insert(make_pair(res, SimplifyVE::ResData(cl1, cl2, v, 1)));
              NDBG(cout << " new, remembered" << endl;);
              // check for early termination: 
              if (local_dd.size() >= (unsigned)gain) {
                NDBG(cout << "-eliminate_var(): too many resolvents, aborted." << endl;);
                return false;
              }
            }
          }
        }
        ++pcl2;
      }
      ++pcl1;
    }
    NDBG(cout << "=eliminate_var(): got up to " << local_dd.size() 
        << " new resolvents" << endl;);
    // now, let's make a list of original clauses that are subsumed by the new
    // resolvents -- note that there's a chance we will have dublicates, and
    // so our final count will be too optimistic, but we'll just have to live
    // with it
    BasicClauseVector p_subs;     // potentially subsumed clauses
    for (SimplifyVE::DerivData::iterator iter = local_dd.begin(); 
         iter != local_dd.end(); ++iter)
      calculate_subsumed(sv, iter->first, p_subs);
    NDBG(cout << "=eliminate_var(): detected up to " << p_subs.size()
        << " clauses subsumed by the new resolvents." << endl;);
    // elimiate if net gain is non-positive
    gain = (int)local_dd.size() - gain - p_subs.size();
    NDBG(cout << "=eliminate_var(): estimated gain is " << gain << endl;);
    if (gain > 0) {
      NDBG(cout << "-eliminate_var(): the gain is positive, not eliminated." 
          << endl;);
      return false;
    }
    // ok, elimiate for real
    NDBG(cout << "=eliminate_var(): eliminating for real" << endl;);
    remove_all(sv, v);
    ++r_vars;
    NDBG(cout << "=eliminate_var(): adding resolvents for real" << endl;);
    for (SimplifyVE::DerivData::iterator iter = local_dd.begin(); 
         iter != local_dd.end(); ++iter) {
      BasicClause* cl = iter->first;
      SimplifyVE::ResData& rd = iter->second;
      NDBG(cout << "  trying " << *cl << " ... ";);
      // check if the clause is already in the set -- if not, add it and its 
      // derivation data; if yes, then it must be in the derivation data with the
      // same variable v -- then, simply increment the counter
      BasicClause* old_cl = gs.lookup_clause(cl);
      if (old_cl == NULL) {
        // clause is not there, add;
        // TEMP: take the group id of the new clause to be the same as its
        // (unique) clause ID -- this is not correct for group-MUS
        assert(!sv.group_mode());
        assert(cl->asize() > 1); // because units were already added
        gs.add_clause(cl);
        gs.set_cl_grp_id(cl, cl->get_id());
        dd.insert(make_pair(cl, rd));
        ++a_clauses;
        NDBG(cout << "new, added" << endl;);
        // put the variables onto the queue for re-processing
        for (CLiterator plit = cl->abegin(); plit != cl->aend(); ++plit)
          vheap.update(abs(*plit));
      } else {
        SimplifyVE::DerivData::iterator iter = dd.find(old_cl);
        assert(iter != dd.end() && iter->second.v == v);
        ++iter->second.count;
        NDBG(cout << "old, incremented resolution count" << endl;);
      }
    }
    NDBG(cout << "=eliminate_var(): removing subsumed clauses" << endl;);
    for (BasicClauseVector::iterator pcl = p_subs.begin(); pcl != p_subs.end(); 
         ++pcl) {
      if (!(*pcl)->removed())
        remove_clause(sv, *pcl);
    }
    sv.trace().push_back(v);
    NDBG(cout << "-elinimate_var(): done with " << v 
        << ", a_clauses = " << a_clauses << ", r_clauses = " << r_clauses << endl;);
    return true;
  }

  /* Performs self-subsumption on clause c
   * @pre c \in gs
   * @body {
   *   foreach l \in lit(c) {
   *     c1 <- c[l = -l]
   *     foreach c2 \in occs(gs, -l) {
   *       if (subsumes(c1, c2)) {
   *         r <- resolve_and_add(gs, c1, c2);
   *         remove_subsumed(gs, r);    // may remove more
   *       }
   *     }
   *   }
   * }
   * @post c' \in gs' & clash(c, c') = {l} -> !subsumes(c R c', c')  
   */
  void self_subsume(SimplifyVE& sv, const BasicClause* c, ClauseQueue& cqueue)
  {
    NDBG(cout << "+self_subsume(): "; c->dump(); cout << endl;);
    BasicGroupSet& gs = sv.md().gset();
    SimplifyVE::DerivData& dd = sv.dd();
    BasicClause* cc = const_cast<BasicClause*> (c); // to flip literals
    for (Literator pl = cc->abegin(); pl != cc->aend(); ++pl) {
      NDBG(cout << "  checking literal " << *pl << endl;);
      *pl = -*pl; // flip      
      cc->update_abstr(); 
      BasicClauseList& cls = gs.occs_list().clauses(*pl);
      for (BasicClauseList::iterator pcl = cls.begin(); pcl != cls.end(); ) {
        if ((*pcl)->removed()) {
          pcl = cls.erase(pcl);
          continue;
        }
        if (subsumes(cc, *pcl)) {
          NDBG(cout << "  found self-resolvent " << **pcl << flush);
          // make resolvent from the candidate
          vector<LINT> lits;
          for (CLiterator plit = (*pcl)->abegin(); plit != (*pcl)->aend(); ++plit)
            if (*plit != *pl)
              lits.push_back(*plit);
          BasicClause* res = gs.make_clause(lits);
          NDBG(cout << ", made " << *res << flush;);
          // check if the clause is already in the set -- if not, add it and its 
          // derivation data; otherwise ignore  
          BasicClause* old_res = gs.lookup_clause(res);
          if (old_res == NULL) {
            assert(!sv.group_mode());
            gs.add_clause(res);
            gs.set_cl_grp_id(res, res->get_id());
            dd.insert(make_pair(res, SimplifyVE::ResData(cc, *pcl, abs(*pl), 1)));
            ++a_clauses;
            NDBG(cout << " new, added; " << flush;);
            if (res->asize() == 1) {
              // unit clause -- queue
              NDBG(cout << " unit, queueued (TODO); ";);
              cqueue.insert(res);
            }
          } else {
            NDBG(cout << "already there, ignored;" << flush;);
          }
          // remove all subsumed by res -- this will remove *pcl
          remove_subsumed(sv, res);
          NDBG(cout << " removed subsumed." << endl;);
          assert((*pcl)->removed()); // should be removed
          pcl = cls.erase(pcl); // remove from occlist
        } else {
          ++pcl;
        }
      }
      *pl = -*pl; // restore
      cc->update_abstr(); 
    }
    NDBG(cout << "-self_subsume(): done" << endl;);
  }

} // anonymous namespace
