//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        solver_llni_factory.cc
 *
 * Description: Low level non-incremental SAT solver object factory implementation.
 *
 * Author:      antonb
 * 
 *                                              Copyright (c) 2013, Anton Belov
\*----------------------------------------------------------------------------*/
//jpms:ec

#include "solver_llni_factory.hh"
#include "minisat-hmuc_llni_wrapper.hh"
#include "picosat_llni_wrapper.hh"

SATSolverLowLevelNonIncrWrapper* SATSolverLLNIFactory::instance_ptr(SATSolverConfig& config) 
{
  if (solver != NULL) { return solver; }
  if (config.get_incr_mode()) {             // *Must* run in non-incremental mode
    tool_abort("Invalid SAT solver selection in factory: must be non-incremental.");
  }
  if (config.chk_sat_solver("minisat-hmuc")) {       // SAT solver must be minisat
    solver = (SATSolverLowLevelNonIncrWrapper*) new MinisatHMUCLowLevelNonIncrWrapper(imgr);
  } else if (config.chk_sat_solver("picosat")) {     // or picosat
    solver = (SATSolverLowLevelNonIncrWrapper*) new PicosatLowLevelNonIncrWrapper(imgr);
  } else {
    tool_abort("Invalid SAT solver selection in factory: unsupported solver.");
  }
  assert(solver != NULL);
  solver->set_verbosity(config.get_verbosity());
  return solver;
}

/*----------------------------------------------------------------------------*/
