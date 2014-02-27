//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        lingeling_ll_wrapper.hh
 *
 * Description: Low-level incremental wrapper for Ligeling SAT solver.
 *
 * Author:      antonb
 * 
 *                                               Copyright (c) 2013, Anton Belov
\*----------------------------------------------------------------------------*/
//jpms:ec

#pragma once

// full paths required to make sure the correct versions are picked up in case
// there are more than one
#include "../extsrc/lingeling-ala/lglib.hh"
#include "solver_ll_wrapper.hh"

class SATSolverLLFactory;

//jpms:bc
/*----------------------------------------------------------------------------*\
 * Class: LingelingLowLevelWrapper
 *
 * Purpose: Provides low-level incremental interface to Ligeling
 *
 \*----------------------------------------------------------------------------*/
//jpms:ec

class LingelingLowLevelWrapper : public SATSolverLowLevelWrapper {
  friend class SATSolverLLFactory;

public:

  // Direct interface

  virtual void init_run(void);      // Initialize data structures for SAT run

  virtual SATRes solve(void);       // Call SAT solver

  virtual void reset_run(void);     // Clean up data structures from SAT run

  virtual void reset_solver(void);  // Clean up all internal data structures

  virtual ULINT nvars(void) { return (ULINT) LingelingALA::lglmaxvar(solver); }

  virtual ULINT ncls(void) { return num_cls; }

  // Config

  void set_phase(ULINT var, LINT ph) {
    if ((ph == 0) || (ph == 1))
      LingelingALA::lglsetphase(solver, ph ? var : -var);
    else if (ph == 3)
      LingelingALA::lglresetphase(solver, var);
  }

  virtual void set_max_conflicts(LINT mconf) {
    tool_abort("set_max_conflicts() is not implemented in this solver.");
  }

  virtual void set_random_seed(ULINT seed) {
    LingelingALA::lglsetopt(solver, "seed", (int)seed);
  }

  // Handle assumptions

  virtual void set_assumption(ULINT svar, LINT sval) {
    DBG(cout<<"[lingeling] set assumption: " << svar << " w/ value: " << sval << endl);
    assumps.push_back(sval ? (int)svar : -(int)svar);
  }

  virtual void set_assumptions(IntVector& assumptions) {
    for(LINT ass : assumptions) { assumps.push_back((int)ass); }
  }

  virtual void clear_assumptions() { assumps.clear(); }


  // Add/remove clauses or clause sets

  virtual void add_clause(ULINT svar, IntVector& litvect) {
    _add_clause(svar, litvect.begin(), litvect.end());
  }

  virtual void add_clause(ULINT svar, BasicClause* cl) {
    _add_clause(svar, cl->begin(), cl->end());
  }

  // Preprocessing (optional)

  virtual bool is_preprocessing(void) { return true; }

  virtual SATRes preprocess(bool turn_off = false);

  virtual void freeze_var(ULINT var) { LingelingALA::lglfreeze(solver, var); }

  virtual void unfreeze_var(ULINT var) { LingelingALA::lglmelt(solver, var); }

  // Additional functionality (optional)

  virtual void get_solver_clauses(BasicClauseSet& cset);

  // Raw access (optional)

  virtual void* get_raw_solver_ptr(void) { return solver; }

protected:

  // Constructor/destructor

  LingelingLowLevelWrapper(IDManager& _imgr);

  virtual ~LingelingLowLevelWrapper(void);

protected:

  // Auxiliary functions

  void handle_sat_outcome(void);

  void handle_unsat_outcome(void);

  void _add_clause(ULINT svar, Literator pbegin, Literator pend);

protected:

  LingelingALA::LGL* solver = nullptr;

  std::vector<int> assumps;

  int num_cls = 0;

  bool pre_on = true;

};

/*----------------------------------------------------------------------------*/
