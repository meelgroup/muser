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

using namespace std;

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

  // populate the test group-set from the results of in MUSData
  ToolConfig test_config;
  test_config.set_sat_solver("minisat"); // config.get_sat_solver());
  test_config.set_refine_clset_mode();
  test_config.set_rmr_mode();
  if (config.get_grp_mode()) test_config.set_grp_mode();

  BasicGroupSet test_gs(test_config);        // group-set used for testing
  if (gs.gexists(0))
    add_clauses(gs, test_gs, 0);
  // the rest
  for (GIDSetCIterator pgid = md.nec_gids().begin(); 
       pgid != md.nec_gids().end(); ++pgid)
    add_clauses(gs, test_gs, *pgid);

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
