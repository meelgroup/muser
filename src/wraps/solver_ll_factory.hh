//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        solver_ll_factory.hh
 *
 * Description: Low level SAT solver object factory.
 *
 * Author:      jpms
 * 
 *                                     Copyright (c) 2012, Joao Marques-Silva
\*----------------------------------------------------------------------------*/
//jpms:ec

#pragma once

#include "solver_config.hh"
#include "solver_ll_wrapper.hh"

//jpms:bc
/*----------------------------------------------------------------------------*\
 * Class: SATSolverLLFactory
 *
 * Purpose: Creates a SAT solver object given configuration
\*----------------------------------------------------------------------------*/
//jpms:ec

class SATSolverLLFactory {

public:

  SATSolverLLFactory(IDManager& _imgr) : imgr(_imgr), solver(NULL) { }

  virtual ~SATSolverLLFactory() {
    if (solver != NULL) { delete solver; solver = NULL; } }

  SATSolverLowLevelWrapper* instance_ptr(SATSolverConfig& config);

  SATSolverLowLevelWrapper& instance_ref(SATSolverConfig& config);
  //{ return *instance_ptr(config); }

  void release() { if (solver != NULL) { delete solver; solver = NULL; } }

protected:

  IDManager& imgr;

  SATSolverLowLevelWrapper* solver;

};

/*----------------------------------------------------------------------------*/
