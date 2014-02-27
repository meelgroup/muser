//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        solver_sls_factory.hh
 *
 * Description: SLS SAT solver object factory (1-to-1 with solver)
 *
 * Author:      jpms
 * 
 *                           Copyright (c) 2012, Anton Belov, Joao Marques-Silva
\*----------------------------------------------------------------------------*/
//jpms:ec

#pragma once

#include "solver_config.hh"
#include "solver_sls_wrapper.hh"

//jpms:bc
/*----------------------------------------------------------------------------*\
 * Class: SATSolverSLSFactory
 *
 * Purpose: Creates a SAT solver object given configuration
\*----------------------------------------------------------------------------*/
//jpms:ec

class SATSolverSLSFactory {

public:

  SATSolverSLSFactory(IDManager& _imgr) : imgr(_imgr), solver(NULL) { }

  virtual ~SATSolverSLSFactory() {
    if (solver != NULL) { delete solver; solver = NULL; } }

  SATSolverSLSWrapper* instance_ptr(SATSolverConfig& config);

  SATSolverSLSWrapper& instance_ref(SATSolverConfig& config) {
    return *instance_ptr(config);
  }
  
  void release() { if (solver != NULL) { delete solver; solver = NULL; } }

protected:

  IDManager& imgr;

  SATSolverSLSWrapper* solver;

};

/*----------------------------------------------------------------------------*/
