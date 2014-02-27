//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        solver_wrapper_gnonincr.cc
 *
 * Description: An implementation of an adapter class that provides an
 *              incremental group-based interface to an instance of
 *              SATSolverLowLevelNonIncrWrapper.
 *
 * Author:      antonb
 *
 * Notes:
 *
 *                                              Copyright (c) 2013, Anton Belov
\*----------------------------------------------------------------------------*/
//jpms:ec

#include "cl_registry.hh"
#include "solver_wrapper_gnonincr.hh"

using namespace std;

//#define DBG(x) x

/* Initialize all internal data structures */
void SATSolverWrapperGrpNonIncr::init_all(void)
{
  cleanup(); isvalid = false;
}

/* Clean up all internal data structures */
void SATSolverWrapperGrpNonIncr::reset_all(void)
{
  cleanup(); isvalid = false;
}

/* Initialize data structures for SAT run */
void SATSolverWrapperGrpNonIncr::init_run(void)
{
  if (isvalid) { throw std::logic_error("Solver interface is in invalid state."); }
  isvalid = true;
}

/* Clean up data structures from SAT run */
void SATSolverWrapperGrpNonIncr::reset_run(void)
{
  if (!isvalid) { throw std::logic_error("Solver interface is in invalid state."); }
  llwrap.reset_run();
  llwrap.reset_solver();
  isvalid = false;
}

/* Adds all groups in the groupset */
void SATSolverWrapperGrpNonIncr::add_groups(BasicGroupSet& gset, bool g0final)
{
  for_each(gset.gbegin(), gset.gend(), [&](GID gid) {
    add_group(gset, gid, (gid == 0) && g0final); });
  update_maxgid(gset.max_gid());
}

/* Adds a single group from the groupset; if final = true the group is
 * added as final right away.
 */
void SATSolverWrapperGrpNonIncr::add_group(BasicGroupSet& gset, GID gid, bool final)
{
  for (auto cl : gset.gclauses(gid))
    if (!cl->removed()) { solver_add_clause(cl, final); }
  update_maxgid(gid);
  DBG(cout << "Added group gid " << gid << endl;);
}

/* Activates (non-final) group */
void SATSolverWrapperGrpNonIncr::activate_group(GID gid)
{
  auto p = g2st_map.find(gid);
  assert(p != end(g2st_map) && (p->second == -1)); // exists, not final, not active
  p->second = 1;
  DBG(cout << "Act. gid " << gid << endl;);
}

/* Deactivates (non-final) group */
void SATSolverWrapperGrpNonIncr::deactivate_group(GID gid)
{
  auto p = g2st_map.find(gid);
  assert(p != end(g2st_map) && (p->second == 1)); // exists, not final, active
  p->second = -1;
  DBG(cout << "Deact. gid " << gid << endl;);
}

/* Returns true if either final, or non-final and active */
bool SATSolverWrapperGrpNonIncr::is_group_active(GID gid)
{
  auto p = g2st_map.find(gid);
  assert(p != end(g2st_map)); // exists
  return p->second >= 0;
}

/* Removes (non-final) group */
void SATSolverWrapperGrpNonIncr::del_group(GID gid)
{
  auto p = g2st_map.find(gid);
  assert(p != g2st_map.end() && (p->second != 0)); // exists, non-final
  g2st_map.erase(p);
  DBG(cout << "Del. gid " << gid << endl;);
}

/* Finalizes a group */
void SATSolverWrapperGrpNonIncr::make_group_final(GID gid)
{
  auto p = g2st_map.find(gid);
  assert(p != g2st_map.end()); // exists
  p->second = 0;
  DBG(cout << "Fin. gid " << gid << endl;);
}

/* True if group is final */
bool SATSolverWrapperGrpNonIncr::is_group_final(GID gid)
{
  auto p = g2st_map.find(gid);
  assert(p != g2st_map.end()); // exists
  return p->second == 0;
}

/// most important stuff is in the helpers ...

/* Cleanup */
void SATSolverWrapperGrpNonIncr::cleanup(void)
{
  g2st_map.clear(); cset.clear(); f_cset.clear(); units.clear(); gcore.clear();
}

/* Solve the current set of clauses instance */
SATRes SATSolverWrapperGrpNonIncr::solve(const IntVector* assump)
{
  if (!isvalid)
    throw std::logic_error("Solver interface is in invalid state.");
  llwrap.init_solver();
  // add all clauses from final or active groups
  for (auto cl : cset) {
    if (cl->removed()) { continue; }
    auto p = g2st_map.find(cl->get_grp_id());
    if (p == end(g2st_map)) { continue; } // group has been removed
    if (p->second == 0) { llwrap.add_untraceable_clause(cl); }
    else if (p->second == 1) { llwrap.add_clause(cl); }
  }
  // add all other clauses
  for (auto cl : f_cset) { llwrap.add_untraceable_clause(cl); }
  _clits.resize(1);
  for (auto lit : units) {
    _clits[0] = lit;
    llwrap.add_untraceable_clause(_clits);
  }
  // add assumptions ...
  if (assump != nullptr) {
    for (auto lit : *assump) {
      _clits[0] = lit;
      llwrap.add_untraceable_clause(_clits);
    }
  }
  llwrap.init_run();
  SATRes res = llwrap.solve();
  if (res == SAT_False) { // build the group core
    BasicClauseVector& core = llwrap.get_unsat_core();
    for (auto cl : core) {
      auto gid = cl->get_grp_id();
      if (gcore.count(gid)) { continue; }
      if (f_cset.find(cl) != end(f_cset)) { continue; }
      auto p = g2st_map.find(gid);
      if (p == end(g2st_map)) { continue; }
      if (p->second == 1) { gcore.insert(gid); }
    }
  }
  return res;
}

/* Add a clause to the internal clause list; if final = true, but the group-ID
 * of the clause is not 0, then the clause is added to the f_cset
 */
void SATSolverWrapperGrpNonIncr::solver_add_clause(BasicClause* cl, bool final)
{
  GID gid = cl->get_grp_id();
  if (gid != gid_Undef && !g2st_map.count(gid)) {
    g2st_map.insert({ gid, gid == 0 ? 0 : 1 }); // active
    update_maxgid(gid);
  }
  // NOTE: we cannot rely on clauses being around until the solve() call -- so,
  // make a copy (often not really created, due to the clause registry), and
  // insert a copy instead
  BasicClauseSet& cs = (final && gid != 0) ? f_cset : cset;
  BasicClause* new_cl = cs.create_clause(cs.get_cl_lits(cl));
  cs.set_cl_grp_id(new_cl, gid);
  DBG(cout << "Added " << (final ? "final " : "") << "clause " << cl; cl->dump();
      cout << "size=" << f_cset.size();
      cout << endl;);
}

/* Add (assert) a unit clause */
void SATSolverWrapperGrpNonIncr::solver_assert_unit_clause(LINT lit)
{
  units.push_back(lit);
}


/*----------------------------------------------------------------------------*/
