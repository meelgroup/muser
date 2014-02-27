//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        solver_factory.hh
 *
 * Description: SAT solver object factory.
 *
 * Author:      antonb; original: jmps
 *
 * Notes:       this is a MUSer2 factory that creates group wrappers around the
 *              low-level wrappers from wraps/
 * 
 *                      Copyright (c) 2010-2013, Joao Marques-Silva, Anton Belov
\*----------------------------------------------------------------------------*/
//jpms:ec

#pragma once

#include "solver_config.hh"
#include "solver_wrapper.hh"
#include "solver_ll_factory.hh"
#include "solver_llni_factory.hh"
#include "solver_sls_factory.hh"

namespace MUSer2 {

//jpms:bc
/*----------------------------------------------------------------------------*\
 * Class: SATSolverFactory
 *
 * Purpose: Creates a SAT solver object given configuration
\*----------------------------------------------------------------------------*/
//jpms:ec

class SATSolverFactory {

public:

  SATSolverFactory(IDManager& imgr)
    : _imgr(imgr), _ll_fact(imgr), _llni_fact(imgr), _sls_fact(imgr) {}

  virtual ~SATSolverFactory(void) {
    if (_solver != nullptr) { delete _solver; _solver = nullptr; }
  }

  MUSer2::SATSolverWrapper& instance(SATSolverConfig& config);

  void release(void) {
    if (_solver != nullptr) { delete _solver; _solver = nullptr; }
  }

protected:

  MUSer2::SATSolverWrapper* _solver = nullptr;

  IDManager& _imgr;

  SATSolverLLFactory _ll_fact;          // for incremental solvers

  SATSolverLLNIFactory _llni_fact;      // for non-incremental solvers

  SATSolverSLSFactory _sls_fact;        // for SLS solvers

};

}
/*----------------------------------------------------------------------------*/
