/*----------------------------------------------------------------------------*\
 * File:        bcp_simplifier.cc
 *
 * Description: Implementation of BCP-based simplifier worker.
 *
 * Author:      antonb
 * 
 * Notes:
 *
 *                                              Copyright (c) 2011, Anton Belov
\*----------------------------------------------------------------------------*/

#ifdef NDEBUG
//#undef NDEBUG // enable assertions (careful !)
#endif

#include <cassert>
#include <iostream>
#include <queue>
#include <vector>
#include "mtl/queue.hh"           // minisat's queue
#include "basic_group_set.hh"
#include "bcp_simplifier.hh"
#include "types.hh"

using namespace std;
using namespace __gnu_cxx;

//#define DBG(x) x

namespace {

  /* A queue of (effectively) unit clauses -- used for propagation
   */
  class PropQueue {
  public:       
    /* Insert into back */
    void insert(LINT lit) { _queue.insert(lit); }
    /* Get the front */
    LINT front(void) { return _queue.peek(); }
    /* Pop the front */
    void pop(void) { return _queue.pop(); }
    /* Size */
    size_t size(void) { return _queue.size(); }
  private:      
    Minisat::Queue<LINT> _queue;       // TODO: try STL also
  };

  /* True is the literal and the value is in conflict */
  bool conflicts(LINT lit, int value) 
  { 
    return ((lit > 0) && (value < 0)) || ((lit < 0) && (value > 0));
  }

  /* Eneuques literal: checks for conflict, makes assignment, puts on queue. 
   * Returns 0 in case of conflict, 1 in case its a new assignment, 2 o/w */
  unsigned enqueue_lit(SimplifyBCP& sb, PropQueue& q, BasicClause* cl, LINT lit) 
  {
    SimplifyBCP::VarData& vd = sb.var_data(abs(lit));
    if (vd.value) { // already assigned
      if (conflicts(lit, vd.value)) { // conflict
        sb.set_conflict_clause(cl);
        assert(cl->asize() == 1);
        cl->shrink(); // conflict clause will have size 0
        sb.set_completed();
        return 0;
      }
      return 2;
    }
    vd.value = (lit > 0) ? 1 : -1;
    vd.reason = cl;
    q.insert(lit);
    ++sb.ua_count();
    return 1;
  }

} // anonymous namespace


/* Handles the SimplifyBCP work item
 */
bool BCPSimplifier::process(SimplifyBCP& sb)
{
  bool group_mode = sb.group_mode();
  DBG(cout << "+BCPSimplifier::process()" << endl;);
  MUSData& md = sb.md();
  BasicGroupSet& gs = md.gset();
  OccsList& o_list = gs.occs_list();

  // grab the write lock right away ...
  md.lock_for_writing(); 

  double t_start = RUSAGE::read_cpu_time();

  // propagation queue; rules:
  //   - the clause used to propagate a new unit is checked for being false 
  //     (i.e. conflict) *before* enqueuing
  //   - if the clause is not coflicting, assignment is made when is literal 
  //     is put on the queue
  // 
  // TODO: clean up the code; how about dealing with SAT clauses at enqueue time ?
  // this way the code for initial units and the rest is exactly the same.
  //
  PropQueue p_queue;

  // enqueue initial units
  for (BasicClauseVector::const_iterator pcl = gs.units().begin(); 
       pcl != gs.units().end(); ++pcl) {
    if (group_mode && (*pcl)->get_grp_id()) // only g0 units
      continue;
    unsigned res = enqueue_lit(sb, p_queue, *pcl, *(*pcl)->abegin());
    if (!res) {
      DBG(cout << "  conflict among initial units." << endl;);
      return sb.completed();
    }
  }   
  DBG(cout << "  " << p_queue.size() << " initial units. " << endl;);

  // off we go ...
  while(p_queue.size() > 0) {
    LINT lit = p_queue.front();
    DBG(cout << "  got " << lit << " from the queue, ";
        SimplifyBCP::VarData& vd = sb.var_data(abs(lit));
        if (vd.reason == NULL) cout << "top-level" << endl;
        else { cout << "reason: "; vd.reason->dump(); cout << endl; });
    // SAT clauses: all clauses with lit are satisfied -- remove them, and 
    // clean up the list on the way ...
    BasicClauseList& sclauses = o_list.clauses(lit);
    for (BasicClauseList::iterator pscl = sclauses.begin(); pscl != sclauses.end(); ) {
      BasicClause* scl = *pscl;
      if (!scl->removed()) {
        DBG(cout << "    clause "; scl->dump(); cout << " is SAT; removing." << endl;);
        scl->mark_removed();
        o_list.update_active_sizes(scl);
        ++sb.rcl_count();
        GID gid = scl->get_grp_id();
        if (--(gs.a_count(gid)) == 0) { // group is gone
          md.r_gids().insert(gid);
          md.r_list().push_front(gid);
          ++sb.rg_count();
        }       
      }
      pscl = sclauses.erase(pscl);
    }
    // Clauses with -lit need to be updated. Update happens as follows:
    //  - the clause is shrunk by one literal
    //  - if the clause is now unit *and* is allowed to propagate, then enqueue
    BasicClauseList& clauses = o_list.clauses(-lit);    
    for (BasicClauseList::iterator pcl = clauses.begin(); pcl != clauses.end(); ) {
      BasicClause* cl = *pcl;
      DBG(cout << "    checking "; cl->dump(); cout << ": " << flush;);
      if (cl->removed()) {
        DBG(cout << "removed earlier, skipping" << endl;);
        pcl = clauses.erase(pcl);
        continue;
      }
      assert(cl->asize() > 0);
      // Note: it is possible that the active size of clause is 1. However,
      // this may only happen in group mode with a non-g0 clause -- if this
      // is the case we have a conflict between g0 and the group of cl,
      // however, since it doesn't tell us anything about whether or the group 
      // is necessary, we're just going to shrink the clause to size 0.
      assert(!(cl->asize() == 1) || (group_mode && cl->get_grp_id()));
      // shrink the clause: move the false literal (-lit) towards the end
      Literator pf = cl->abegin(); // find false literal
      while (pf != cl->aend() && *pf != -lit) ++pf;
      assert(pf != cl->aend()); 
      Literator pu = cl->aend()-1; // unassigned literal - the one before last
      if (pf != pu) { // literals are not the same
        swap(*pf, *pu);
        // check if the clause might have become unsorted
        if (!cl->unsorted()) {
          // note that pf is now points to the unassigned literal
          if ((pf != cl->abegin()) && (abs(*pf) < abs(*(pf-1))))
            cl->mark_unsorted();
          if ((pf != pu - 1) && (abs(*pf) > abs(*(pf+1))))
            cl->mark_unsorted();
        }
      }
      cl->shrink();
      DBG(cout << "new asize = " << cl->asize() << ": " << *cl << endl;);
      // ok, now if the clause is unit, *and*, if we're in the group-mode is g0, 
      // then propagate ...
      if ((cl->asize() == 1) && (!group_mode || !cl->get_grp_id())) {
        LINT prop_lit = *cl->abegin();
        DBG(cout << "    clause is unit and can be propagated, enqueuing " 
            << prop_lit << flush;);
        unsigned res = enqueue_lit(sb, p_queue, cl, prop_lit);
        if (!res) {
          DBG(cout << "  conflict." << endl;);
          return sb.completed();
        }
        DBG(cout << ((res == 1) ? " done." : " already assigned, skipped") << endl;);
      }
      // done with this clause        
      ++pcl;
    }
    // neither lit nor -lit does not occur anywhere anymore (lit is done already)
    assert(o_list.clauses(lit).empty());
    assert(o_list.active_size(lit) == 0);
    o_list.clauses(-lit).clear(); 
    o_list.active_size(-lit) = 0;
    // done with lit
    p_queue.pop();
    DBG(cout << "  finished with " << lit << endl;);
  }
  // done
  sb.cpu_time() = RUSAGE::read_cpu_time() - t_start;    
  md.release_lock();
  sb.set_completed();
  DBG(cout << "-BCPSimplifier::process()." << endl;);
  return sb.completed();
}

/* Reconstructs the solution (inside sb.md()) in terms of the original
 * clauses. This is only relevant for non-group mode, and will result in
 * the "undoing" of BCP on all necessary clauses. 'sb' is expected to
 * be the instance used during the last call to process()
 */
void BCPSimplifier::reconstruct_solution(SimplifyBCP& sb)
{
  MUSData& md = sb.md();
  DBG(cout << "+BCPSimplifier::reconstruct_solution()." << endl;);
  if (sb.conflict()) {
    // in the group-mode conflict implies that the MUS is empty; in the non-group
    // mode, the conflict needs to be traced back -- the support of the empty
    // clause is the output MUS
    md.make_empty_gmus();
    if (!sb.group_mode()) { // the only clause is the conflict clause
      md.nec_gids().insert(sb.conflict_clause()->get_id());
      DBG(cout << "  top-level conflict, left only: "; 
          sb.conflict_clause()->dump(); cout << endl;);
    }
  }
  // in group mode, there's nothing to do anymore
  if (sb.group_mode())
    return;
  // now, calculate support of all necessary clauses: very easy -- all units are
  // stored in var_data, so for each false literal that appears in a necessary
  // clause, take its reason, and process the reason's literals in the same manner
  queue<BasicClause*> q;
  for (GIDSet::const_iterator pgid = md.nec_gids().begin(); pgid != md.nec_gids().end(); 
       ++pgid) {
    assert(md.gset().gclauses(*pgid).size() == 1);
    BasicClause* pcl = *md.gset().gclauses(*pgid).begin();
    if (pcl->asize() != pcl->size())
      q.push(pcl);
  }
  while (!q.empty()) {
    BasicClause* pcl = q.front();
    for (CLiterator plit = pcl->aend(); plit != pcl->end(); ++plit) {
      SimplifyBCP::VarData& vd = sb.var_data(abs(*plit));
      assert(vd.value != 0); // must be assigned
      q.push(vd.reason);
    }
    DBG(if (pcl->size() != pcl->asize()) {
        cout << "  restored "; pcl->dump(); cout << endl;});
    pcl->restore();
    pcl->mark_unsorted(); // probably
    pcl->unmark_removed();
    md.r_gids().erase(pcl->get_grp_id()); // for handling conflict (empty MUS)
    md.nec_gids().insert(pcl->get_grp_id());
    q.pop();
  }
  // done
  DBG(cout << "-BCPSimplifier::reconstruct_solution()." << endl;);
}

//
// ------------------------  Local implementations  ----------------------------
//

namespace {

} // anonymous namespace
