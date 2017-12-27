//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        solver_ll_factory.cc
 *
 * Description: Low level SAT solver object factory implementation.
 *
 * Author:      jpms
 * 
 *                                     Copyright (c) 2012, Joao Marques-Silva
\*----------------------------------------------------------------------------*/
//jpms:ec

#include "solver_ll_factory.hh"
#include "minisat22_ll_wrapper_incr.hh"
#include "minisat-hmuc_ll_wrapper.hh"
#include "picosat_ll_wrapper.hh"
#include "minisat-abbr_ll_wrapper.hh"
#include "minisat-gh_ll_wrapper.hh"
#include "glucose30_ll_wrapper.hh"
#include "lingeling_ll_wrapper.hh"

#ifdef IPASIR_LIB // use only if we currently compile against IPASIR
#include "IPASIR_ll_wrapper.hh"
#endif

/** Implementation of the factory method */
SATSolverLowLevelWrapper* SATSolverLLFactory::instance_ptr(SATSolverConfig& config) 
{
  if (solver != NULL) { return solver; }
  if (!config.get_incr_mode()) {             // *Must* run in incremental mode
    tool_abort("Invalid non-incremental SAT solver selection in factory");
  }
  if (config.chk_sat_solver("minisat")) {
    solver = (SATSolverLowLevelWrapper*) new Minisat22LowLevelWrapper(imgr);
  } else if (config.chk_sat_solver("minisats")) {
    solver = (SATSolverLowLevelWrapper*) new Minisat22sLowLevelWrapper(imgr);
  } else if (config.chk_sat_solver("minisat-hmuc")) {
    solver = (SATSolverLowLevelWrapper*) new MinisatHMUCLowLevelWrapper(imgr);
  } else if (config.chk_sat_solver("picosat")) {
    solver = (SATSolverLowLevelWrapper*) new PicosatLowLevelWrapper(imgr);
  } else if (config.chk_sat_solver("minisat-abbr")) {
    solver = (SATSolverLowLevelWrapper*) new MinisatAbbrLowLevelWrapper(imgr);
  } else if (config.chk_sat_solver("minisat-gh")) {
    solver = (SATSolverLowLevelWrapper*) new MinisatGHLowLevelWrapper(imgr);
  } else if (config.chk_sat_solver("minisat-ghs")) {
    solver = (SATSolverLowLevelWrapper*) new MinisatGHsLowLevelWrapper(imgr);
  } else if (config.chk_sat_solver("glucose")) {
    solver = (SATSolverLowLevelWrapper*) new Glucose30LowLevelWrapper(imgr);
  } else if (config.chk_sat_solver("glucoses")) {
    solver = (SATSolverLowLevelWrapper*) new Glucose30sLowLevelWrapper(imgr);
  } else if (config.chk_sat_solver("lingeling")) {
    solver = (SATSolverLowLevelWrapper*) new LingelingLowLevelWrapper(imgr);
#ifdef IPASIR_LIB
  } else if (config.chk_sat_solver("ipasir")) {
    solver = (SATSolverLowLevelWrapper*) new IPASIRLowLevelWrapper(imgr);
#endif
  } else {
    tool_abort("Invalid SAT solver selection in factory: unsupported solver");
  }
  assert(solver != NULL);
  solver->set_verbosity(config.get_verbosity());
  return solver;
}

SATSolverLowLevelWrapper& SATSolverLLFactory::instance_ref(SATSolverConfig& config)
{
  return *instance_ptr(config);
}

/*----------------------------------------------------------------------------*/
