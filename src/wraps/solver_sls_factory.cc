//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        solver_sls_factory.cc
 *
 * Description: SLS SAT solver object factory (1-to-1 with solver) implementation
 *
 * Author:      antonb
 * 
 *                           Copyright (c) 2012, Anton Belov, Joao Marques-Silva
\*----------------------------------------------------------------------------*/
//jpms:ec

#include "solver_sls_factory.hh"
#include "ubcsat12_sls_wrapper.hh"

SATSolverSLSWrapper* SATSolverSLSFactory::instance_ptr(SATSolverConfig& config) 
{
  if (solver != NULL) { return solver; }
  // TODO:  use configuration to select a solver
  solver = (SATSolverSLSWrapper*) new Ubcsat12SLSWrapper(imgr);
  assert(solver != NULL);
  solver->set_verbosity(config.get_verbosity());
  return solver;
}


/*----------------------------------------------------------------------------*/
