//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        solver_factory.cc
 *
 * Description: SAT solver object factory implementation
 *
 * Author:      antonb; original: jmps
 *
 * Notes:       this is a MUSer2 factory that creates group wrappers around the
 *              low-level wrappers from wraps/
 * 
 *                      Copyright (c) 2010-2013, Joao Marques-Silva, Anton Belov
\*----------------------------------------------------------------------------*/
//jpms:ec

#include "solver_factory.hh"
#include "solver_wrapper_gincr.hh"
#include "solver_wrapper_gnonincr.hh"
#include "solver_wrapper_gsls.hh"

/*----------------------------------------------------------------------------*\
 * Purpose: the factory method that creates a SAT solver object given
 * configuration
\*----------------------------------------------------------------------------*/
//jpms:ec

MUSer2::SATSolverWrapper& MUSer2::SATSolverFactory::instance(SATSolverConfig& config)
{
  if (_solver != nullptr) { return *_solver; }

  if (config.get_incr_mode()) { // in incremental mode
    _solver = new SATSolverWrapperGrpIncr(_imgr, _ll_fact.instance_ref(config));
  } else {
    if (config.get_sls_mode()) {
      _solver = new SATSolverWrapperGrpSLS(_imgr, _sls_fact.instance_ref(config));
    } else {
      _solver = new SATSolverWrapperGrpNonIncr(_imgr, _llni_fact.instance_ref(config));
    }
  }
  if (_solver == nullptr)
    tool_abort("invalid SAT solver configuration in group solver factory");
  _solver->set_verbosity(config.get_verbosity());
  return *_solver;
}
