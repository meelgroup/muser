//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        solver_factory.cc
 *
 * Description: SAT solver object factory implementation
 *
 * Author:      jpms, antonb
 * 
 *                      Copyright (c) 2010-2011, Joao Marques-Silva, Anton Belov
\*----------------------------------------------------------------------------*/
//jpms:ec

#include "solver_config.hh"
#include "solver_factory.hh"
#include "solver_wrapper.hh"
// solver-specific wrappers start here ...
#include "minisat22_wrapper_gincr.hh"

/*----------------------------------------------------------------------------*\
 * Purpose: the factory method that creates a SAT solver object given
 * configuration
\*----------------------------------------------------------------------------*/
//jpms:ec

SATSolverWrapper& SATSolverFactory::instance(SATSolverConfig& config)
{
  if (solver != NULL)
    return *solver;

  if (config.get_incr_mode()) { // Run in incremental mode
    // group-based interface is now default
    if (config.chk_sat_solver("minisat")) {
      solver = (SATSolverWrapper*) new Minisat22WrapperGrpIncr(imgr);
    } else if (config.chk_sat_solver("minisats")) {
      solver = (SATSolverWrapper*) new Minisat22SWrapperGrpIncr(imgr);
    } else {
      tool_abort("Selection of invalid incremental SAT solver in factory");
    }
  }
  if (solver == NULL)
    tool_abort("Selection of invalid SAT solver in factory");
  solver->set_verbosity(config.get_verbosity());
  return *solver;
}
