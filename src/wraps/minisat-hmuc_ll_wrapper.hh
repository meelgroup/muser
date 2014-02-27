//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        minisat-hmuc_ll_wrapper.hh
 *
 * Description: Low-level incremental wrapper for MinisatHMUC
 *
 * Author:      antonb
 * 
 *                                               Copyright (c) 2013, Anton Belov
\*----------------------------------------------------------------------------*/
//jpms:ec

#pragma once

// full paths required to make sure the correct versions are picked up
#include "../extsrc/minisat-hmuc/mtl/Vec.h"
#include "../extsrc/minisat-hmuc/core/SolverTypes.h"
#include "../extsrc/minisat-hmuc/core/Solver.h"
#include "../extsrc/minisat-hmuc/simp/SimpSolver.h"
#include "solver_ll_wrapper.hh"

class SATSolverLLFactory;

//jpms:bc
/*----------------------------------------------------------------------------*\
 * Class: MinisatHMUCLowLevelWrapper
 *
 * Purpose: Provides low-level incremental interface to Minisat-HMUC.
 *
 \*----------------------------------------------------------------------------*/
//jpms:ec

class MinisatHMUCLowLevelWrapper : public SATSolverLowLevelWrapper {
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
    solver->setPolarity(var, (ph<0));
  }

  virtual void set_random_seed(ULINT seed) {
    solver->random_seed = seed ? (double)seed/MAXULINT : 0.131008300313;
  }

  // Handle assumptions

  void set_assumption(ULINT svar, LINT sval) {
    DBG(cout<<"[minisat-hmuc] set assumption: " << svar << " w/ value: " << sval<<endl);
    assumps.push(MinisatHMUC::mkLit(svar, sval==0));
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

  virtual bool is_preprocessing(void) { return true; }

  virtual SATRes preprocess(bool turn_off = false);

  virtual void freeze_var(ULINT var) { solver->setFrozen(var, true); }

  virtual void unfreeze_var(ULINT var) { solver->setFrozen(var, false); }

  // Raw access (optional)

  virtual void* get_raw_solver_ptr(void) { return solver; }

protected:

  // Constructor/destructor

  MinisatHMUCLowLevelWrapper(IDManager& _imgr);

  virtual ~MinisatHMUCLowLevelWrapper(void);

protected:

  // Auxiliary functions

  void handle_sat_outcome(void);

  void handle_unsat_outcome(void);

  inline void update_maxvid(LINT nvid) {
    assert(nvid > 0);
    while(nvid >= solver->nVars()) { solver->newVar((bool)(phase == -1), true); }
    assert(solver->nVars() > nvid);  // Always +1 var in solver
  }

  // Compute unsat core given map of assumptions to clauses
  void compute_unsat_core(void);

  void _add_clause(ULINT svar, Literator pbegin, Literator pend);

protected:

  MinisatHMUC::SimpSolver* solver = nullptr;

  MinisatHMUC::vec<MinisatHMUC::Lit> clits;

  MinisatHMUC::vec<MinisatHMUC::Lit> assumps;

};

/*----------------------------------------------------------------------------*/
