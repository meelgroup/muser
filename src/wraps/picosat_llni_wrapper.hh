//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        picosat_llni_wrapper.hh
 *
 * Description: Low-level non-incremental wrapper for re-entrant Picosat (v953+)
 *
 * Author:      antonb
 * 
 *                                               Copyright (c) 2013, Anton Belov
\*----------------------------------------------------------------------------*/
//jpms:ec

#pragma once

#include <vector>
#include "picosat-tr.hh"
#include "solver_llni_wrapper.hh"

class SATSolverLLNIFactory;

//jpms:bc
/*----------------------------------------------------------------------------*\
 * Class: PicosatLowLevelNonIncrWrapper
 *
 * Purpose: Provides low-level non-incremental interface to re-entrant Picosat
 * (v953+).
 *
 \*----------------------------------------------------------------------------*/
//jpms:ec

class PicosatLowLevelNonIncrWrapper : public SATSolverLowLevelNonIncrWrapper {
  friend class SATSolverLLNIFactory;

public:
  // Direct interface (see base for comments)

  virtual void init_solver(void);

  virtual void init_run(void);

  virtual SATRes solve(void);

  virtual void reset_run(void);

  virtual void reset_solver(void);

  virtual ULINT nvars(void) { return (ULINT) Picosat954TR::picosat_variables(solver) + 1; }

  virtual ULINT ncls(void) { return (ULINT) Picosat954TR::picosat_added_original_clauses(solver); }

  // Config

  void set_phase(ULINT var, LINT ph) {
    if (ph) { Picosat954TR::picosat_set_default_phase_lit(solver, var, ph); }
  }

  virtual void set_max_conflicts(LINT mconf) {
    tool_abort("set_max_conflicts() is not implemented in this solver.");
  }

  virtual void set_random_seed(ULINT seed) {
    Picosat954TR::picosat_set_seed(solver, (unsigned)seed);
  }

  // Add/remove clauses or clause sets

  virtual void add_clause(BasicClause* cl) { _add_clause(cl); }

  /** This is a special version of add_clause that allows to tell the SAT
   * solver to skip some of the literals of the clause when loading it to the
   * the SAT solver. This can be used to skip selector literals, for example.
   * The lit_test should return true if the literal should be skipped, false
   * otherwise.
   */
  virtual void add_clause(BasicClause* cl, std::function<bool(LINT lit)> skip_lit) {
    _add_clause(cl, &skip_lit);
  }

  virtual void add_clause(IntVector& clits) { _add_clause(clits.begin(), clits.end()); }

  virtual void add_clauses(BasicClauseSet& rclset) {
    for (BasicClause* cl : rclset) { _add_clause(cl); }
  }

  // Output (optional)

  virtual void print_cnf(const char* fname) {
    FILE* out = fopen(fname, "w");
    Picosat954TR::picosat_print(solver, out);
    fclose(out);
  }

  // Additional functionality (optional)

  /** Removes the specified percentage (0-100) of less active learned clauses
   * from the underlying solver.
   */
  virtual void remove_learned(int pct) {
    Picosat954TR::picosat_remove_learned(solver, pct);
  }

  /** If implemented, allows to clean up some of the state in the underlying
   * solver, e.g. activity scores, stored phases, etc. This is solver-dependent.
   */
  virtual void cleanup_solver(void) {
    Picosat954TR::picosat_reset_phases(solver);
    Picosat954TR::picosat_reset_scores(solver);
  }

  // Raw access (optional)

  virtual void* get_raw_solver_ptr(void) { return solver; }

protected:

  // Constructor/destructor

  PicosatLowLevelNonIncrWrapper(IDManager& _imgr);

  virtual ~PicosatLowLevelNonIncrWrapper(void);

protected:

  // Auxiliary functions

  int _add_clause(BasicClause* cl, std::function<bool(LINT lit)>* skip_lit = nullptr);
  int _add_clause(Literator pbegin, Literator pend,
                  std::function<bool(LINT lit)>* skip_lit = nullptr);

  void handle_sat_outcome(void);

  void handle_unsat_outcome(void);

protected:

  Picosat954TR::PicoSAT* solver = nullptr;

  std::vector<BasicClause*> id2cl;          // map: index is id of a clause

};

/*----------------------------------------------------------------------------*/
