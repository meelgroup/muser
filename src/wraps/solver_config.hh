//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        solver_config.hh
 *
 * Description: Specifies required interface of SAT solver wrappers.
 *
 * Author:      jpms
 * 
 *                                     Copyright (c) 2010, Joao Marques-Silva
\*----------------------------------------------------------------------------*/
//jpms:ec

#ifndef _SOLVER_CONFIG_H
#define _SOLVER_CONFIG_H 1

#include "id_manager.hh"

//jpms:bc
/*----------------------------------------------------------------------------*\
 * Typedefs.
\*----------------------------------------------------------------------------*/
//jpms:ec

// Existing unsat-based releases of MSUnCore
typedef enum SAT_Solvers {
  PicosatSolver = 0xA001,
  MinisatSolver = 0xA002
} SATSolver;


//jpms:bc
/*----------------------------------------------------------------------------*\
 * Class: SATSolverConfig
 *
 * Purpose: Required SAT solver configuration interface.
\*----------------------------------------------------------------------------*/
//jpms:ec

class SATSolverConfig {

public:

  SATSolverConfig(void) { }

  virtual ~SATSolverConfig(void) { }

  virtual bool chk_sat_solver(const char* _solver) = 0;

  virtual bool get_incr_mode(void) = 0;

  virtual int get_verbosity(void) = 0;

  virtual bool get_grp_mode(void) { return false; }

  virtual bool get_trace_enabled(void) { return false; }

  virtual bool get_sls_mode(void) { return false; }

};

#endif /* _SOLVER_CONFIG_H */

/*----------------------------------------------------------------------------*/
