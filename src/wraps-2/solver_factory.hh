//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        solver_factory.hh
 *
 * Description: SAT solver object factory.
 *
 * Author:      jpms
 * 
 *                                     Copyright (c) 2010, Joao Marques-Silva
\*----------------------------------------------------------------------------*/
//jpms:ec

#ifndef _SOLVER_FACTORY_H
#define _SOLVER_FACTORY_H 1

#include "solver_config.hh"
#include "solver_wrapper.hh"

//jpms:bc
/*----------------------------------------------------------------------------*\
 * Class: SATSolverFactory
 *
 * Purpose: Creates a SAT solver object given configuration
\*----------------------------------------------------------------------------*/
//jpms:ec

class SATSolverFactory {

public:

  SATSolverFactory(IDManager& _imgr) : imgr(_imgr), solver(NULL) {}

  virtual ~SATSolverFactory(void) {
    if (solver != NULL) { delete solver; solver = NULL; }
  }

  SATSolverWrapper& instance(SATSolverConfig& config);

  void release() {
    if (solver != NULL) { delete solver; solver = NULL; }
  }

protected:

  IDManager& imgr;

  SATSolverWrapper* solver;

};

#endif /* _SOLVER_FACTORY_H */

/*----------------------------------------------------------------------------*/
