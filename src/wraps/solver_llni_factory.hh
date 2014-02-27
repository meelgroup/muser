//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        solver_llni_factory.hh
 *
 * Description: Low level non-incremental SAT solver object factory.
 *
 * Author:      antonb
 * 
 *                                              Copyright (c) 2013, Anton Belov
\*----------------------------------------------------------------------------*/
//jpms:ec

#pragma once

#include "solver_config.hh"
#include "solver_llni_wrapper.hh"

//jpms:bc
/*----------------------------------------------------------------------------*\
 * Class: SATSolverLLNIFactory
 *
 * Purpose: Creates a SAT solver object given configuration
\*----------------------------------------------------------------------------*/
//jpms:ec

class SATSolverLLNIFactory {

public:

  SATSolverLLNIFactory(IDManager& _imgr) : imgr(_imgr), solver(NULL) { }

  virtual ~SATSolverLLNIFactory(void) {
    if (solver != NULL) { delete solver; solver = NULL; } }

  SATSolverLowLevelNonIncrWrapper* instance_ptr(SATSolverConfig& config);

  SATSolverLowLevelNonIncrWrapper& instance_ref(SATSolverConfig& config) {
    return *instance_ptr(config);
  }

  void release() { if (solver != NULL) { delete solver; solver = NULL; } }

protected:

  IDManager& imgr;

  SATSolverLowLevelNonIncrWrapper* solver;

};

/*----------------------------------------------------------------------------*/
