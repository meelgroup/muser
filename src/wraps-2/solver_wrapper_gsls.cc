//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        solver_wrapper_gsls.cc
 *
 * Description: An implementation of an adapter class that provides an
 *              incremental group-based interface to an instance of
 *              SATSolverLowLevelSLSWrapper.
 *
 * Author:      antonb
 *
 * Notes:
 *
 *                                              Copyright (c) 2013, Anton Belov
\*----------------------------------------------------------------------------*/
//jpms:ec

#include "cl_registry.hh"
#include "solver_wrapper_gsls.hh"

using namespace std;

//#define DBG(x) x

/* Initialize all internal data structures */
void SATSolverWrapperGrpSLS::init_all(void)
{
  cleanup(); isvalid = false;
}

/* Clean up all internal data structures */
void SATSolverWrapperGrpSLS::reset_all(void)
{
  cleanup(); isvalid = false;
}

/* Initialize data structures for SAT run */
void SATSolverWrapperGrpSLS::init_run(void)
{
  if (isvalid) { throw std::logic_error("Solver interface is in invalid state."); }
  llwrap.set_verbosity(verbosity);
  llwrap.set_weighted(false);
  llwrap.set_max_tries(10);
  llwrap.set_cutoff(cutoff/10);
  llwrap.set_timeout(timeout);
  llwrap.set_algo_adaptnovelty_plus(0.01);
  //llwrap.set_algo_captain_jack();
  llwrap.init_all();
  isvalid = true;
}

/* Clean up data structures from SAT run */
void SATSolverWrapperGrpSLS::reset_run(void)
{
  if (!isvalid) { throw std::logic_error("Solver interface is in invalid state."); }
  llwrap.reset_all();
  isvalid = false;
}

/* Adds all groups in the groupset */
void SATSolverWrapperGrpSLS::add_groups(BasicGroupSet& gset, bool g0final)
{
  for_each(gset.gbegin(), gset.gend(), [&](GID gid) {
    add_group(gset, gid, (gid == 0) && g0final); });
  update_maxgid(gset.max_gid());
}

/* Adds a single group from the groupset; if final = true the group is
 * added as final right away.
 */
void SATSolverWrapperGrpSLS::add_group(BasicGroupSet& gset, GID gid, bool final)
{
  for (auto cl : gset.gclauses(gid))
    if (!cl->removed()) { solver_add_clause(cl, final); }
  update_maxgid(gid);
  DBG(cout << "Added group gid " << gid << endl;);
}

/* Activates (non-final) group */
void SATSolverWrapperGrpSLS::activate_group(GID gid)
{
  auto p = g2st_map.find(gid);
  assert(p != end(g2st_map) && (p->second == -1)); // exists, not final, not active
  p->second = 1;
  DBG(cout << "Act. gid " << gid << endl;);
}

/* Deactivates (non-final) group */
void SATSolverWrapperGrpSLS::deactivate_group(GID gid)
{
  auto p = g2st_map.find(gid);
  assert(p != end(g2st_map) && (p->second == 1)); // exists, not final, active
  p->second = -1;
  DBG(cout << "Deact. gid " << gid << endl;);
}

/* Returns true if either final, or non-final and active */
bool SATSolverWrapperGrpSLS::is_group_active(GID gid)
{
  auto p = g2st_map.find(gid);
  assert(p != end(g2st_map)); // exists
  return p->second >= 0;
}

/* Removes (non-final) group */
void SATSolverWrapperGrpSLS::del_group(GID gid)
{
  auto p = g2st_map.find(gid);
  assert(p != g2st_map.end() && (p->second != 0)); // exists, non-final
  g2st_map.erase(p);
  DBG(cout << "Del. gid " << gid << endl;);
}

/* Finalizes a group */
void SATSolverWrapperGrpSLS::make_group_final(GID gid)
{
  auto p = g2st_map.find(gid);
  assert(p != g2st_map.end()); // exists
  p->second = 0;
  DBG(cout << "Fin. gid " << gid << endl;);
}

/* True if group is final */
bool SATSolverWrapperGrpSLS::is_group_final(GID gid)
{
  auto p = g2st_map.find(gid);
  assert(p != g2st_map.end()); // exists
  return p->second == 0;
}

/// most important stuff is in the helpers ...

/* Cleanup */
void SATSolverWrapperGrpSLS::cleanup(void)
{
  g2st_map.clear(); cset.clear(); f_cset.clear(); units.clear(); gcore.clear();
}

/* Solve the current set of clauses instance */
SATRes SATSolverWrapperGrpSLS::solve(const IntVector* assump)
{
  if (!isvalid)
    throw std::logic_error("Solver interface is in invalid state.");
  // add all clauses from final or active groups
  for (auto cl : cset) {
    if (cl->removed()) { continue; }
    auto p = g2st_map.find(cl->get_grp_id());
    if (p == end(g2st_map)) { continue; } // group has been removed
    if (p->second >= 0) { llwrap.add_clause(cl); }
  }
  // add all other clauses
  for (auto cl : f_cset) { llwrap.add_clause(cl); }
  _clits.resize(1);
  for (auto lit : units) {
    _clits[0] = lit;
    llwrap.add_clause(_clits, 1);
  }
  // add assumptions ...
  if (assump != nullptr) {
    for (auto lit : *assump) {
      _clits[0] = lit;
      llwrap.add_clause(_clits, 1);
    }
  }
  llwrap.init_run();
  SATRes res = llwrap.solve();
  if (res == SAT_False) {
    // in case it happens, pretend that everything is a core
    for (auto cl : cset) {
      if (cl->removed()) { continue; }
      auto p = g2st_map.find(cl->get_grp_id());
      if (p->second == 1) { gcore.insert(cl->get_grp_id()); }
    }
  } else { // get the model or approximation
    llwrap.get_assignment(_model);
  }
  llwrap.reset_run();
  return res;
}

/* Add a clause to the internal clause list; if final = true, but the group-ID
 * of the clause is not 0, then the clause is added to the f_cset
 */
void SATSolverWrapperGrpSLS::solver_add_clause(BasicClause* cl, bool final)
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
  DBG(cout << "Added " << (final ? "final " : "") << "clause "; cl->dump();
      cout << ", f_cset size=" << f_cset.size();
      cout << endl;);
}

/* Add (assert) a unit clause */
void SATSolverWrapperGrpSLS::solver_assert_unit_clause(LINT lit)
{
  units.push_back(lit);
}


/*----------------------------------------------------------------------------*/
