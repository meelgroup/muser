//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        IPASIR_ll_wrapper.hh
 *
 * Description: Low-level incremental wrapper for re-entrant IPASIR
 *
 * Author:      nmanthey
 *
 *                                          Copyright (c) 2017, Norbert Manthey
\*----------------------------------------------------------------------------*/
//jpms:ec

#pragma once

#ifdef IPASIR_LIB

#include "solver_ll_wrapper.hh"

// use IPASIR, if selected
extern "C" {
#include "ipasir.h"
}

class SATSolverLLFactory;

//jpms:bc
/*----------------------------------------------------------------------------*\
 * Class: IPASIRLowLevelWrapper
 *
 * Purpose: Provides low-level incremental interface to re-entrant IPASIR.
 *
 \*----------------------------------------------------------------------------*/
//jpms:ec

class IPASIRLowLevelWrapper : public SATSolverLowLevelWrapper {
  friend class SATSolverLLFactory;

public:

  // Direct interface

  virtual void init_run(void);      // Initialize data structures for SAT run

  virtual SATRes solve(void);       // Call SAT solver

  virtual void reset_run(void);     // Clean up data structures from SAT run

  virtual void reset_solver(void);  // Clean up all internal data structures

  virtual ULINT nvars(void) { return (ULINT) max_var; } // Picosat954::picosat_variables(solver) + 1; }

  virtual ULINT ncls(void) { return (ULINT) added_clauses; } // Picosat954::picosat_added_original_clauses(solver); }

  // Config

  void set_phase(ULINT var, LINT ph) {
//    if (ph) { Picosat954::picosat_set_default_phase_lit(solver, var, ph); }
  }

  virtual void set_max_conflicts(LINT mconf) {
    tool_abort("set_max_conflicts() is not implemented in this solver.");
  }

  virtual void set_random_seed(ULINT seed) {
//    Picosat954::picosat_set_seed(solver, (unsigned)seed);
  }

  // Handle assumptions

  virtual void set_assumption(ULINT svar, LINT sval) {
    DBG(cout<<"[picosat] set assumption: " << svar << " w/ value: " << sval << endl);
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

  // Additional functionality (optional)

  /** Removes the specified percentage (0-100) of less active learned clauses
   * from the underlying solver.
   */
  virtual void remove_learned(int pct) {
//     Picosat954::picosat_remove_learned(solver, pct);
  }

  /** If implemented, allows to clean up some of the state in the underlying
   * solver, e.g. activity scores, stored phases, etc. This is solver-dependent.
   */
  virtual void cleanup_solver(void) {
//    Picosat954::picosat_reset_phases(solver);
//    Picosat954::picosat_reset_scores(solver);
  }

  // Raw access (optional)

  virtual void* get_raw_solver_ptr(void) { return solver; }

protected:

  // Constructor/destructor

  IPASIRLowLevelWrapper(IDManager& _imgr);

  virtual ~IPASIRLowLevelWrapper(void);

protected:

  // Auxiliary functions

  void handle_sat_outcome(void);

  void handle_unsat_outcome(void);

  void _add_clause(ULINT svar, Literator pbegin, Literator pend);

protected:

  void* solver = nullptr;

  // IPASIR does not count itself
  int max_var = 0;
  int added_clauses = 0;

  std::vector<int> assumps;

};

#endif

/*----------------------------------------------------------------------------*/
