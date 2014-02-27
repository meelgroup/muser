//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        glucose30_ll_wrapper.hh
 *
 * Description: Low-level incremental wrapper for glucose30 solver.
 *
 * Author:      antonb
 * 
 * Note:        the temptation to have a single template class for all Minisat
 *              derivatives was fought off on the grounds that sometimes it is
 *              desirabe to quickly hack one specific solver/wrapper, and also
 *              that some Minisat mods have extra functionality that others
 *              don't.
 *
 *                                               Copyright (c) 2013, Anton Belov
\*----------------------------------------------------------------------------*/
//jpms:ec

#pragma once

// full paths required to make sure the correct versions are picked up
#include "../extsrc/glucose30/mtl/Vec.h"
#include "../extsrc/glucose30/core/SolverTypes.h"
#include "../extsrc/glucose30/core/Solver.h"
#include "../extsrc/glucose30/simp/SimpSolver.h"
#include "solver_ll_wrapper.hh"

class SATSolverLLFactory;

//jpms:bc
/*----------------------------------------------------------------------------*\
 * Class: Glucose30LowLevelWrapperTmpl
 *
 * Purpose: Provides low-level incremental interface to glucose30. This is
 * a template that specializes to either Glucose::Solver or to
 * Glucose::SimpSolver.
 *
 \*----------------------------------------------------------------------------*/
//jpms:ec

template<class S>
class Glucose30LowLevelWrapperTmpl : public SATSolverLowLevelWrapper {
  friend class SATSolverLLFactory;

public:

  // Direct interface

  void init_run();                  // Initialize data structures for SAT run

  SATRes solve();                   // Call SAT solver

  void reset_run();                 // Clean up data structures from SAT run

  void reset_solver();              // Clean up all internal data structures

  ULINT nvars() { return (ULINT) solver->nVars(); }

  ULINT ncls() { return (ULINT) (solver->nClauses()+solver->nLearnts()); }

  // Config

  virtual void set_phase(ULINT var, LINT ph) {
    assert(var < nvars());
    solver->setPolarity((int)var, ph < 0);
  }

  virtual void set_random_seed(ULINT seed) {
    solver->random_seed = seed ? (double)seed/MAXULINT : 0.131008300313;
  }

  virtual void set_max_problem_var(ULINT pvar) {
    solver->setIncrementalMode();
    solver->initNbInitialVars((int)pvar);
  }

  // Handle assumptions

  void set_assumption(ULINT svar, LINT sval) {
    DBG(cout<<"[glucose30] set assumption: " << svar << " w/ value: " << sval<<endl);
    assumps.push(Glucose::mkLit(svar, sval==0));
  }

  void set_assumptions(IntVector& assumptions) {
    for(LINT ass : assumptions) {
      set_assumption((ULINT) std::llabs(ass), (ass<0)?0:1);
    }
  }

  void clear_assumptions() { assumps.clear(); }


  // Add/remove clauses or clause sets

  virtual void add_clause(ULINT svar, IntVector& litvect) {
    _add_clause(svar, litvect.begin(), litvect.end());
  }

  virtual void add_clause(ULINT svar, BasicClause* cl) {
    _add_clause(svar, cl->begin(), cl->end());
  }

  // Preprocessing -- please read the comments in solver_ll_wrapper.hh !

  virtual bool is_preprocessing(void) { return simp; }

  virtual SATRes preprocess(bool turn_off = false);

  virtual void freeze_var(ULINT var);

  virtual void unfreeze_var(ULINT var);

  // Raw access (optional)

  virtual void* get_raw_solver_ptr(void) { return solver; }

protected:

  // Constructor/destructor

  Glucose30LowLevelWrapperTmpl(IDManager& _imgr);

  virtual ~Glucose30LowLevelWrapperTmpl(void);

protected:

  // Auxiliary functions

  void handle_sat_outcome(void);

  void handle_unsat_outcome(void);

  inline void update_maxvid(LINT nvid) {
    assert(nvid > 0);
    while(nvid >= solver->nVars())
      solver->newVar((phase == -1), true);
    assert(solver->nVars() > nvid);  // Always +1 var in solver
  }

  // Compute unsat core given map of assumptions to clauses
  void compute_unsat_core(void);

  void _add_clause(ULINT svar, Literator pbegin, Literator pend);

protected:

  S* solver = nullptr;

  bool simp = false;                            // set to true when simplifying

  Glucose::vec<Glucose::Lit> clits;

  Glucose::vec<Glucose::Lit> assumps;

};

/*----------------------------------------------------------------------------*\
 * Class: Glucose30LowLevelWrapper
 *
 * Purpose: Provides low-level incremental interface to glucose30.
\*----------------------------------------------------------------------------*/
typedef Glucose30LowLevelWrapperTmpl<Glucose::Solver> Glucose30LowLevelWrapper;

/*----------------------------------------------------------------------------*\
 * Class: Glucose30sLowLevelWrapper
 *
 * Purpose: Provides low-level incremental interface to glucose30 with SATElite
 *
 * Note: might be buggy !
\*----------------------------------------------------------------------------*/
typedef Glucose30LowLevelWrapperTmpl<Glucose::SimpSolver> Glucose30sLowLevelWrapper;

/*----------------------------------------------------------------------------*/
