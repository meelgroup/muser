//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        solver_wrapper_gincr.cc
 *
 * Description: An implementation of an adapter class that provides an
 *              incremental group-based interface to an instance of
 *              SATSolverLowLevelWrapper.
 *
 * Author:      antonb
 *
 * Notes:
 *
 *                                              Copyright (c) 2013, Anton Belov
\*----------------------------------------------------------------------------*/
//jpms:ec

#include "solver_wrapper_gincr.hh"

using namespace std;

//#define DBG(x) x

/* Initialize all internal data structures */
void SATSolverWrapperGrpIncr::init_all(void)
{
  gcore.clear(); g2a_map.clear(); a2g_map.clear();
  maxgid = 0; isvalid = false;
}

/* Clean up all internal data structures */
void SATSolverWrapperGrpIncr::reset_all(void)
{
  llwrap.reset_solver();
  gcore.clear(); g2a_map.clear(); a2g_map.clear(); isvalid = false;
}

/* Initialize data structures for SAT run */
void SATSolverWrapperGrpIncr::init_run(void)
{
  if (isvalid) { throw std::logic_error("Solver interface is in invalid state."); }
  llwrap.init_run();
  gcore.clear();
  isvalid = true;
}

/* Clean up data structures from SAT run */
void SATSolverWrapperGrpIncr::reset_run(void)
{
  if (!isvalid) { throw std::logic_error("Solver interface is in invalid state."); }
  llwrap.reset_run();
  isvalid = false;
}

/* This assumes that the GID of the clause is set correctly. It will create
 * a new assumption variable if the GID has not been yet seen. This method
 * will  not check if the clause is marked as removed. Group 0 clauses are
 * added as final.
 */
void SATSolverWrapperGrpIncr::add_clause(BasicClause* cl)
{
  ULINT aid = 0; // assumption lit
  GID gid = cl->get_grp_id();
  if (gid) {
    // make new indicator, if needed
    auto p = g2a_map.find(gid);
    if (p != end(g2a_map)) {
      aid = labs(p->second);
    } else {
      aid = (cl->get_slit()) ? cl->get_slit() : imgr.new_id();
      g2a_map.insert({ gid, -aid });
      a2g_map.insert({ aid, gid });
    }
  }
  DBG(cout << "Adding clause "; cl->dump(); cout << "slit=" << cl->get_slit() << ", alit: " << aid << endl;);
  solver_add_clause(cl, aid);
  update_maxgid(gid);
}

/* Adds a final clause (non-removable) */
void SATSolverWrapperGrpIncr::add_final_clause(BasicClause* cl)
{
  DBG(cout << "Adding final clause "; cl->dump(); cout << endl;);
  solver_add_clause(cl);
}

/* Adds a final unit clause (non-removable) */
void SATSolverWrapperGrpIncr::add_final_unit_clause(LINT lit)
{
  DBG(cout << "Adding final unit " << lit << endl;);
  solver_assert_unit_clause(lit);
}

/* Adds all groups in the groupset */
void SATSolverWrapperGrpIncr::add_groups(BasicGroupSet& gset, bool g0final)
{
  for_each(gset.gbegin(), gset.gend(), [&](GID gid) {
    add_group(gset, gid, (gid == 0) && g0final); });
  update_maxgid(gset.max_gid());
}

/* Adds a single group from the groupset; if final = true the group is
 * added as final right away.
 */
void SATSolverWrapperGrpIncr::add_group(BasicGroupSet& gset, GID gid, bool final)
{
  if (gset.a_count(gid)) {
    ULINT aid = 0; // assumption lit
    bool added = false;
    for (auto cl : gset.gclauses(gid))
      if (!cl->removed()) {
        if (aid == 0 && !final) { aid = (cl->get_slit()) ? cl->get_slit() : imgr.new_id(); }
        solver_add_clause(cl, aid);
        added = true;
      }
    if (aid && added) {
      g2a_map.insert({ gid, -aid });
      a2g_map.insert({ aid, gid });
      DBG(cout << "Added group gid " << gid << ", alit=" << aid << endl;);
    }
    update_maxgid(gid);
  }
  if (!gid && final) { has_g0 = true; }
}

/* Activates (non-final) group */
void SATSolverWrapperGrpIncr::activate_group(GID gid)
{
  auto p = g2a_map.find(gid);
  assert(p != end(g2a_map)); // exists
  assert(p->second > 0); // not final, not active
  p->second = -p->second;
  DBG(cout << "Act. gid " << gid << " (alit " << labs(p->second) << ")" << endl;);
}

/* Deactivates (non-final) group */
void SATSolverWrapperGrpIncr::deactivate_group(GID gid)
{
  auto p = g2a_map.find(gid);
  assert(p != end(g2a_map)); // exists
  assert(p->second < 0); // not final, active
  p->second = -p->second;
  DBG(cout << "Deact. gid " << gid << " (alit " << labs(p->second) << ")" << endl;);
}

/* Returns true if either final, or non-final and active */
bool SATSolverWrapperGrpIncr::is_group_active(GID gid)
{
  auto p = g2a_map.find(gid);
  assert(p != end(g2a_map)); // exists
  return (p->second <= 0);
}

/* Removes (non-final) group */
void SATSolverWrapperGrpIncr::del_group(GID gid)
{
  auto p = g2a_map.find(gid);
  assert(p != g2a_map.end()); // exists
  if (p->second)
    solver_assert_unit_clause(labs(p->second));  // assert assumption literal
  DBG(cout << "Del. gid " << gid << " (alit " << labs(p->second) << ")" << endl;);
  a2g_map.erase(labs(p->second));
  g2a_map.erase(p);
}

/* Finalizes a group */
void SATSolverWrapperGrpIncr::make_group_final(GID gid)
{
  auto p = g2a_map.find(gid);
  assert(p != g2a_map.end()); // exists
  solver_assert_unit_clause(-labs(p->second));  // Cancel assumption literal
  DBG(cout << "Fin. gid " << gid << " (alit " << labs(p->second) << ")" << endl;);
  p->second = 0;
}

/* True if group is final */
bool SATSolverWrapperGrpIncr::is_group_final(GID gid)
{
  auto p = g2a_map.find(gid);
  assert(p != g2a_map.end()); // exists
  return p->second == 0;
}

/* Returns the activation literal for group -- setting to true makes the
 * group inactive; 0 means the group has been finalized. */
LINT SATSolverWrapperGrpIncr::get_group_activation_lit(GID gid)
{
  auto p = g2a_map.find(gid);
  assert(p != g2a_map.end()); // exists
  return labs(p->second);
}

/// most important stuff is in the helpers ...

/* Solve the current set of clauses instance */
SATRes SATSolverWrapperGrpIncr::solve(const IntVector* assump)
{
  if (!isvalid)
    throw std::logic_error("Solver interface is in invalid state.");
  llwrap.clear_assumptions();
  for (auto p = begin(g2a_map); p != end(g2a_map); ++p)
    if (p->second) { llwrap.set_assumption(abs(p->second), p->second > 0); }
  if (assump != nullptr) { llwrap.set_assumptions(*const_cast<IntVector*>(assump)); }
  SATRes res = llwrap.solve();
  if (res == SAT_False) {
    IntVector& core = llwrap.get_unsat_core();
    for (auto a : core) { assert(a2g_map.count(a)); gcore.insert(a2g_map[a]); }
  }
  return res;
}

/* Add a clause to solver, with associated assumption (0 means no assumption)
 */
void SATSolverWrapperGrpIncr::solver_add_clause(BasicClause* cl, ULINT alit)
{
  // Note: the low-level wrappers add assumptions in the opposite polarity to
  // the way muser2 expects them: negative in wrappers, positive in muser2
  if (alit == 0) {
    llwrap.add_final_clause(cl);
    DBG(cout << "Added final clause: "; cl->dump(); cout << endl;);
    return;
  }
  _clits.resize(cl->asize() + 1);
  auto p = begin(_clits);
  for (auto pcl = cl->abegin(); pcl != cl->aend(); ++pcl, ++p) { *p = *pcl; }
  *p = alit;
  llwrap.add_final_clause(_clits);
  if (llwrap.is_preprocessing()) { llwrap.freeze_var(alit); }
  DBG(cout << "Added clause: "; cl->dump(); cout << ", assumpt: " << alit << endl;);
  return;
}

/* Add (assert) a unit clause */
void SATSolverWrapperGrpIncr::solver_assert_unit_clause(LINT lit)
{
  _clits.resize(1);
  _clits[0] = lit;
  llwrap.add_final_clause(_clits);
}


/*----------------------------------------------------------------------------*/
