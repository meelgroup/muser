/*----------------------------------------------------------------------------*\
 * File:        tester.cc
 *
 * Description: Implementation of correctness tester.
 *
 * Author:      antonb
 * 
 * Notes:
 *
 *                                              Copyright (c) 2012, Anton Belov
\*----------------------------------------------------------------------------*/

#ifdef NDEBUG
#undef NDEBUG // enable assertions (careful !)
#endif

#include <cassert>
#include <deque>
#include <iostream>
#include <queue>
#include <vector>
#include "basic_group_set.hh"
#include "check_unsat.hh"
#include "compute_mus.hh"
#include "mus_data.hh"
#include "mus_extractor.hh"
#include "sat_checker.hh"
#include "solver_wrapper.hh"
#include "tester.hh"
#include "utils.hh"

using namespace std;
using MUSer2::SATSolverWrapper;

//#define DBG(x) x
//#define CHK(x) x

namespace {

  /** Helper to add clauses to test group set */
  void add_clauses(BasicGroupSet& gs, BasicGroupSet& test_gs, GID gid);

} // anonymous namespace


/* Handles the TestMUS work item -- tests (group)MUS for correctness.
 */
bool Tester::process(TestMUS& tm)
{
  DBG(cout << "+Tester::process(TestMUS)" << endl;);
  MUSData& md = tm.md();
  BasicGroupSet& gs = md.gset();

  double t_start = RUSAGE::read_cpu_time();

  // grab the write lock right away ...
  md.lock_for_reading(); 

  // populate the test group-set from the results of in MUSData
  ToolConfig test_config;
  test_config.set_sat_solver("minisat"); // config.get_sat_solver());
  test_config.set_refine_clset_mode();
  //test_config.unset_model_rotate_mode();
  test_config.set_rmr_mode();
  if (config.get_grp_mode()) test_config.set_grp_mode();

  BasicGroupSet test_gs(test_config);        // group-set used for testing
  if (gs.gexists(0))
    add_clauses(gs, test_gs, 0);
  // the rest
  for (GIDSetCIterator pgid = md.nec_gids().begin(); 
       pgid != md.nec_gids().end(); ++pgid)
    add_clauses(gs, test_gs, *pgid);

  md.release_lock();

  // now testing ...
  MUSData test_md(test_gs);
  SATChecker schecker(_imgr, test_config);
  CheckUnsat cu(test_md);
  if (schecker.process(cu) && cu.completed()) {
    if (!cu.is_unsat()) {
      tm._result = TestMUS::SAT;
      tm.set_completed();
    } else {
      MUSExtractor mex(_imgr, test_config);
      mex.set_sat_checker(&schecker);
      ComputeMUS cm(test_md);
      if (mex.process(cm) && cm.completed()) {
        // now the original instance is MUS if all groups are necessary
        tm._result = ((test_md.nec_gids().size() == md.nec_gids().size()) 
                      ? TestMUS::UNSAT_MU : TestMUS::UNSAT_NOTMU);
        tm._sat_calls = mex.sat_calls();
        tm._rot_groups = mex.rot_groups();
        tm._unnec_groups = md.nec_gids().size() - test_md.nec_gids().size();
        tm.set_completed();
      }
    }
  }
  tm._cpu_time = RUSAGE::read_cpu_time() - t_start;    
  DBG(cout << "-Tester::process(TestMUS)." << endl;);
  return tm.completed();
}

/* Handles the TestIrr work item -- tests irrendant subformulas for correctness.
 */
bool Tester::process(TestIrr& ti)
{
  DBG(cout << "+Tester::process(TestIrr)" << endl;);
  MUSData& md = ti.md();
  BasicGroupSet& gs = md.gset();

  double t_start = RUSAGE::read_cpu_time();

  // grab the write lock right away ...
  md.lock_for_reading(); 

  // populate the test group-set from the results of in MUSData
  ToolConfig test_config; // most of defaults are ok
  test_config.set_sat_solver("minisat"); //config.get_sat_solver());
  test_config.set_rm_red_mode();
  test_config.set_imr_mode();
  if (config.get_grp_mode()) test_config.set_grp_mode();

  BasicGroupSet test_gs(test_config);        // group-set used for testing
  if (gs.gexists(0))
    add_clauses(gs, test_gs, 0);
  for (GIDSetCIterator pgid = md.nec_gids().begin(); 
       pgid != md.nec_gids().end(); ++pgid)
    add_clauses(gs, test_gs, *pgid);

  md.release_lock();

  // now testing ...
  MUSData test_md(test_gs);
  SATChecker schecker(_imgr, test_config);
  MUSExtractor mex(_imgr, test_config);
  mex.set_sat_checker(&schecker);
  ComputeMUS cm(test_md);
  if (mex.process(cm) && cm.completed()) {
    // now the original instance is irredundant if all groups are necessary; if
    // if not we're done
    ti._sat_calls = mex.sat_calls();
    ti._red_groups = md.nec_gids().size() - test_md.nec_gids().size();
    if (ti._red_groups != 0) {
      ti._result = TestIrr::RED;
      ti.set_completed();
    } else {
      // ok, now we need to see whether every one of the removed clauses is implied
      // by the irredundant part; we will be re-using the SAT solver -- note 
      // that at this point it has the clauses of the irredundant group set.
      SATSolverWrapper& solver(schecker.solver());
      md.lock_for_reading();
      ti._result = TestIrr::IRRED_CORRECT; // will change on error or incorrectness
      for (GID gid : md.r_gids()) {
        // make a groupset with a single group that contains negation of the clause(s),
        // add it to solver, check unsat, remove it, and so on ...
        BasicGroupSet neg_gs;
        Utils::make_neg_group(gs.gclauses(gid), neg_gs, gid, _imgr); // re-using *pgid
        solver.add_groups(neg_gs);
        solver.init_run();
        SATRes outcome = solver.solve();
        if (outcome == SAT_False) {
          // all good - implied
          DBG(cout << "  clause is implied, good." << endl;);
        } else if (outcome == SAT_True) {
          // not implied
          ti._result = TestIrr::IRRED_INCORRECT;
          ti._nonimpl_groups++;
          DBG(cout << "  clause is NOT implied, results incorrect." << endl;);
        } else {
          ti._result = TestIrr::UNKNOWN;
          DBG(cout << "  error running SAT solver." << endl;);
          break;
        }
        ti._sat_calls++;
        solver.reset_run();
        solver.del_group(gid);
      } // loop on removed gids
      md.release_lock();
      if (ti._result != TestIrr::UNKNOWN)
        ti.set_completed();
    } // done irredundant
  } // done 
  ti._cpu_time = RUSAGE::read_cpu_time() - t_start;    
  DBG(cout << "-Tester::process(TestIrr)." << endl;);
  return ti.completed();
}

/* Handles the TestVMUS work item -- tests VMUS for correctness.
 */
bool Tester::process(TestVMUS& tm)
{
  DBG(cout << "+Tester::process(TestVMUS)" << endl;);
  MUSData& md = tm.md();
  BasicGroupSet& gs = md.gset();

  double t_start = RUSAGE::read_cpu_time();

  // grab the write lock right away ...
  md.lock_for_reading(); 

  ToolConfig test_config;
  test_config.set_sat_solver("minisat"); //config.get_sat_solver());
  test_config.set_var_mode();
  if (config.get_grp_mode())
    test_config.set_grp_mode();
  test_config.unset_refine_clset_mode();
  test_config.unset_model_rotate_mode();

  BasicGroupSet test_gs(test_config);        // group-set used for testing

  // here the tricky part is to create the groupset: we need all clauses 
  // that use only necessary, or group 0, variables; so we're just going
  // to run through all of the original clauses -- if a clause has a removed
  // variable, we will not take it (and its supposed to be removed -- this
  // is an extra check)
  
  // but, first, let's make a set of all "good" variables
  ULINTSet good_vars;
  for (vgset_iterator pvgid = gs.vgbegin(); pvgid != gs.vgend(); ++pvgid) {
    if ((*pvgid == 0) || md.nec(*pvgid)) {
      VarVector& vars = pvgid.vgvars();
      copy(vars.begin(), vars.end(), inserter(good_vars, good_vars.begin()));
    }
  }
  DBG(cout << "Good vars: "; PRINT_ELEMENTS(good_vars);cout << endl;);

  // now, copy clauses
  for (cvec_iterator pcl = gs.begin(); pcl != gs.end(); ++pcl) {
    BasicClause* cl = *pcl;
    bool remove = false;
    for (CLiterator plit = cl->abegin(); !remove && (plit != cl->aend()); ++plit)
      remove = (good_vars.find(abs(*plit)) == good_vars.end());
    CHK(if (remove != cl->removed())
          tool_abort("Tester::process(TestVMUS) -- invalid clause status detected !"););
    if (!remove) {
      // add clause to the new group set
      vector<LINT> lits(cl->asize());
      copy(cl->abegin(), cl->aend(), lits.begin());
      BasicClause* ncl = test_gs.make_clause(lits);
      test_gs.add_clause(ncl);
      test_gs.set_cl_grp_id(ncl, cl->get_grp_id());
    }
  }
  // and, if in the group mode, set variable group ids
  if (config.get_grp_mode()) {
    for (ULINTSet::iterator pgv = good_vars.begin(); pgv != good_vars.end(); ++pgv)
      test_gs.set_var_grp_id(*pgv, gs.get_var_grp_id(*pgv));
  }
  
  md.release_lock();

  // now testing ...
  MUSData test_md(test_gs);
  SATChecker schecker(_imgr, test_config);
  CheckUnsat cu(test_md);
  if (schecker.process(cu) && cu.completed()) {
    if (!cu.is_unsat()) {
      tm._result = TestVMUS::SAT;
      tm.set_completed();
    } else {
      MUSExtractor mex(_imgr, test_config);
      mex.set_sat_checker(&schecker);
      ComputeMUS cm(test_md);
      if (mex.process(cm) && cm.completed()) {
        // now the original instance is MUS if all variable groups are necessary
        tm._result = ((test_md.nec_gids().size() == md.nec_gids().size()) 
                      ? TestVMUS::UNSAT_VMU : TestVMUS::UNSAT_NOTVMU);
        tm._sat_calls = mex.sat_calls();
        tm._rot_groups = mex.rot_groups();
        tm._unnec_groups = md.nec_gids().size() - test_md.nec_gids().size();
        tm.set_completed();
      }
    }
  }
  tm._cpu_time = RUSAGE::read_cpu_time() - t_start;    
  DBG(cout << "-Tester::process(TestVMUS)." << endl;);
  return tm.completed();
}



//
// ------------------------  Local implementations  ----------------------------
//

namespace {

  /** Helper to add clauses to test group set */
  void add_clauses(BasicGroupSet& gs, BasicGroupSet& test_gs, GID gid) 
  {
    BasicClauseVector& gi = gs.gclauses(gid);
    for (BasicClauseVector::iterator pcl = gi.begin(); pcl != gi.end(); ++pcl) {
      BasicClause* ocl = *pcl;
      vector<LINT> lits(ocl->asize());
      copy(ocl->abegin(), ocl->aend(), lits.begin());
      BasicClause* ncl = test_gs.make_clause(lits);
      test_gs.add_clause(ncl);
      test_gs.set_cl_grp_id(ncl, ocl->get_grp_id());
    }
  }

} // anonymous namespace
