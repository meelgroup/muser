//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        solver_ll_wrapper.hh
 *
 * Description: 
 *
 * Author:      jpms
 * 
 *                                     Copyright (c) 2012, Joao Marques-Silva
\*----------------------------------------------------------------------------*/
//jpms:ec

#ifndef _SOLVER_LL_WRAPPER_H
#define _SOLVER_LL_WRAPPER_H 1

#include <algorithm>
#include <stdexcept>
#include "globals.hh"
#include "id_manager.hh"
#include "basic_clset.hh"
#include "solver_utils.hh"

using namespace SolverUtils;

class SATSolverLLFactory;


//jpms:bc
/*----------------------------------------------------------------------------*\
 * Class: SATSolverLowLevelWrapper
 *
 * Purpose: Provides low level incremental interface to SAT solver
\*----------------------------------------------------------------------------*/
//jpms:ec

class SATSolverLowLevelWrapper {
  friend class SATSolverLLFactory;

public:

  // Direct interface

  virtual void init_run() = 0;         // Initialize data structures for SAT run

  virtual SATRes solve() = 0;

  virtual void reset_run() = 0;         // Clean up data structures from SAT run

  virtual void reset_solver() = 0;      // Clean up all internal data structures

  virtual ULINT nvars() = 0;

  virtual ULINT ncls() = 0;

  // Config

  void set_verbosity(int verb) { verbosity = verb; }

  /** Sets the default phase for all variables: -1 = false, 0 = random, 1 = true
   * Do not call this if you want to use solver's default polarity
   */
  void set_def_phase(LINT ph) { phase = ph; }

  /** Sets the phase for individual variable: -1 = false, 0 = random, 1 = true
   * Do not call this if you want to use solver's default polarity
   */
  virtual void set_phase(ULINT var, LINT ph) = 0;

  /** Same as set_phase, except second argument is bool */
  void set_polarity(ULINT var, bool pol) { set_phase(var, pol ? 1 : -1); }

  /** Sets the maximum number of conflicts per call (might be not supported);
   * -1 = no maximum; note that this affects completeness (SAT_NoRes might be
   * returned)
   */
  virtual void set_max_conflicts(LINT mconf) { max_confls = mconf; }

  /** If the solver supports this, sets the output stream to be used for writing
   * out the proof trace in case of UNSAT outcome at the end of solve(); it
   * is up to called to open/close the stream; 0 disables writing
   */
  virtual void set_proof_trace_stream(FILE* o_stream) {
    tool_abort("set_proof_trace_stream() is not implemented for this solver.");
  }

  /** Sets the random seed for the solver
   */
  virtual void set_random_seed(ULINT seed) {
    tool_abort("set_random_seed() is not implemented for this solver.");
  }

  /** Tells the wrapper if it needs to fetch the model; set to false if you
   * don't need the model, might save lots of time; default = true
   */
  void set_need_model(bool nm) { need_model = nm; }

  /** Tells the wrapper if it needs to fetch the core; set to false if you
   * don't need the core, might save lots of time; default = true
   */
  void set_need_core(bool nc) { need_core = nc; }

  /** Some incremental solvers implement optimizations that require the knowledge
   * of which variables are selectors. This method allows to set the largest
   * variable ID of problem variables (i.e. everything above that is a selector)
   */
  virtual void set_max_problem_var(ULINT pvar) { }

  // Handle assumptions

  virtual void set_assumption(ULINT svar, LINT sval) = 0;

  virtual void set_assumptions(IntVector& assumptions) = 0;

  virtual void clear_assumptions() = 0;

  // Access result of SAT solver call (defaults should do)

  virtual IntVector& get_model(void) { return model; }

  virtual void get_model(IntVector& rmodel) { rmodel = model; }

  /** Note: the core is in terms of assumption *variables* (not lits)
   */
  virtual IntVector& get_unsat_core(void) { return ucore; }

  /** Note: the core is in terms of assumption *variables* (not lits)
   */
  virtual void get_unsat_core(IntVector& rucore) { rucore = ucore; }

  // Manipulate clauses in SAT solver

  /** 'svar' = 0 means the clause is final */
  virtual void add_clause(ULINT svar, IntVector& clits) = 0;

  /** 'svar' = 0 means the clause is final */
  virtual void add_clause(ULINT svar, BasicClause* cl=NULL) = 0;

  virtual void add_clauses(IntVector& svars, BasicClauseSet& cset) {
    assert(svars.size() == cset.size());
    IntVector::iterator vi = svars.begin();
    cset_iterator ci = cset.begin();
    for (; vi != svars.end(); ++ci, ++vi) { add_clause(*vi, *ci); }
  }

  virtual void add_final_clause(BasicClause* cl) { add_clause(0, cl); }

  virtual void add_final_clause(IntVector& clits) { add_clause(0, clits); }

  // TODO: 2-nd parameter in the following 4 methods is not used, remove

  virtual void del_clause(ULINT svar, IntVector& clits) { del_clause(svar); }

  virtual void del_clause(ULINT svar, BasicClause* cl = NULL) {
    IntVector lits(1, -svar);
    add_final_clause(lits);
  }

  virtual void del_clauses(IntVector& svars, BasicClauseSet& cset) {
    for (IntVector::iterator iv = svars.begin(); iv != svars.end(); ++iv)
      del_clause(*iv);
  }

  virtual void make_clause_final(ULINT svar, BasicClause* cl) {
    IntVector lits(1, svar);
    add_final_clause(lits);
  }

  // Preprocessing (optional)

  /** Returns true if this solver knows to do preprocessing; if this is overriden,
   * then preprocess() should be implemented, and probably the rest of the methods
   * as well. */
  virtual bool is_preprocessing(void) { return false; }

  /** Preprocesses the loaded instance; depending on the implementation may do
   * nothing, or do a lot of preprocessing, depending on the configuration.
   * NOTE: as opposed to minisat, by default the solver will *not* do
   * preprocessing before SAT solving. Hence the control over preprocessing is
   * in your hands. If turn_off is set to false, then after the preprocess()
   * call is finished, no further pre-processing can be done -- this gives
   * the opportunity for the SAT solver to free up internal data structures.
   * If you know that you will not be using preprocessing at all, you can call
   * preprocess(true) before you add any clauses to the SAT solver.
   * @return SAT_True, SAT_False, SAT_NoRes depending on the results of prepro.
   */
  virtual SATRes preprocess(bool turn_off = false) {
    tool_abort("preprocess() is not implemented for this solver.");
    return SAT_NoRes;
  }

  virtual void freeze_var(ULINT var) {
    tool_abort("freeze_var() is not implemented for this solver.");
  }

  virtual void unfreeze_var(ULINT var) {
    tool_abort("unfreeze_var() is not implemented for this solver.");
  }

  // Output (optional)

  virtual void print_cnf(const char* fname) {
    tool_abort("print_cnf() is not implemented for this solver.");
  }

  // Additional functionality (optional)

  /** Simplifies the instance, e.g. by removing already satisfied clauses
   */
  virtual void simplify(void) {
    tool_abort("simplify() is not implemented for this solver.");
  }

  /** Adds to cset the clauses that are actually inside the underlying
   * SAT solver -- useful for getting the preprocessed instances back.
   */
  virtual void get_solver_clauses(BasicClauseSet& cset) {
    tool_abort("get_solver_clauses() is not implemented for this solver.");
  }

  /** Returns the activity of a variale */
  virtual double get_activity(ULINT var) {
    tool_abort("get_activity() is not implemented in this solver.");
    return 0;
  }

  /** Removes the specified percentage (0-100) of less active learned clauses
   * from the underlying solver.
   */
  virtual void remove_learned(int pct) {
    tool_abort("remove_learned() is not implemented in this solver.");
  }

  /** If implemented, allows to clean up some of the state in the underlying
   * solver, e.g. activity scores, stored phases, etc. This is solver-dependent.
   */
  virtual void cleanup_solver(void) {
    tool_abort("cleanup_solver() is not implemented in this solver.");
  }

  // Raw access (optional)

  /** This method can be implemented to give access to the underlying SAT
   * solver instance. This may be useful to tweak some solver-specific low-level
   * configuation parameters.
   */
  virtual void* get_raw_solver_ptr(void) {
    tool_abort("get_raw_solver_ptr() is not implemented for this solver.");
    return NULL;
  }

protected:

  // Constructor/destructor

  SATSolverLowLevelWrapper(IDManager& _imgr) :
    imgr(_imgr), phase(2), max_confls(-1), verbosity(0), isvalid(false),
    need_model(true), need_core(true) { }

  virtual ~SATSolverLowLevelWrapper() { }


protected:

  IDManager& imgr;

  IntVector model;

  IntVector ucore;

  LINT phase;                  // -1(false), 0(rand), 1(true), 2(solver default)

  LINT max_confls;             // the limit on number of conflicts (-1 = no limit)

  LINT verbosity;

  bool isvalid;

  bool need_model;             // if false, do not fetch the model

  bool need_core;              // if false, do not fetch the core
};

#endif /* _SOLVER_LL_WRAPPER_H */

/*----------------------------------------------------------------------------*/
