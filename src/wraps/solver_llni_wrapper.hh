//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        solver_llni_wrapper.hh
 *
 * Description: low-level non-incremental interface to SAT solver
 *
 * Author:      antonb
 *
 *                                              Copyright (c) 2013, Anton Belov
\*----------------------------------------------------------------------------*/
//jpms:ec

#pragma once

#include <stdexcept>
#include <functional>
#include "globals.hh"
#include "id_manager.hh"
#include "basic_clset.hh"
#include "solver_utils.hh"

using namespace SolverUtils;

class SATSolverLLNIFactory;


//jpms:bc
/*----------------------------------------------------------------------------*\
 * Class: SATSolverLowLevelNonIncrWrapper
 *
 * Purpose: Provides low level non-incremental interface to SAT solver
 *
 * Notes: the sequence of calls to the main iterface must be as follows:
 * (init_solver, ((add_clause, )* init_run, solve, reset_run,)*, reset_solver)*
 * If a solver does not support the addition of clauses after reset_run, you
 * have to call reset_solver and then start from init_solver again.
\*----------------------------------------------------------------------------*/
//jpms:ec

class SATSolverLowLevelNonIncrWrapper {
  friend class SATSolverLLNIFactory;

public:

  // Main interface

  /** Initialize the solver: has to be called before any clauses are added.
   * Note that some non-incremental solvers allow to add clauses between the
   * SAT solver calls; if this is the case, clauses can be added after
   * init_solver(), and before init_run().
   */
  virtual void init_solver(void) = 0;

  /** Prepare for the run: call after init_solver() and before solve(); no
   * clauses can be added, until reset_run is called.
   */
  virtual void init_run(void) = 0;

  /** Solve the current instance */
  virtual SATRes solve(void) = 0;

  /** Clean up the results of the most recent solve(); clauses can be added now
   * for the solvers that support this.
   */
  virtual void reset_run(void) = 0;

  /** This will typically drop all clauses and free up the SAT solver instance */
  virtual void reset_solver(void) = 0;

  virtual ULINT nvars(void) = 0;

  virtual ULINT ncls(void) = 0;

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

  void set_polarity(ULINT var, bool pol) { set_phase(var, pol ? 1 : -1); }

  /* Sets the maximum number of conflicts per call (might be not supported);
   * -1 = no maximum; note that this affects completeness (SAT_NoRes might be
   * returned)
   */
  virtual void set_max_conflicts(LINT mconf) { max_confls = mconf; }

  /* If the solver supports this, sets the output stream to be used for writing
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

  // Access result of SAT solver call (defaults should do)

  virtual IntVector& get_model(void) { return model; }

  virtual void get_model(IntVector& rmodel) { rmodel = model; }

  virtual BasicClauseVector& get_unsat_core(void) { return ucore; }

  virtual void get_unsat_core(BasicClauseVector& rucore) { rucore = ucore; }

  // Add clauses to the SAT solver

  virtual void add_clause(BasicClause* cl) = 0;

  /** This is a special version of add_clause that allows to tell the SAT
   * solver to skip some of the literals of the clause when loading it to the
   * the SAT solver. This can be used to skip selector literals, for example.
   * The lit_test should return true if the literal should be skipped, false
   * otherwise.
   */
  virtual void add_clause(BasicClause* cl, std::function<bool(LINT lit)> skip_lit) {
    tool_abort("add_clause with literal test is not implemented in this solver");
  }

  virtual void add_clause(IntVector& clits) = 0;

  virtual void add_clauses(BasicClauseSet& cset) = 0;

  /** Some solvers might actually support untraceable clauses */
  virtual void add_untraceable_clause(BasicClause* cl) { add_clause(cl); }

  virtual void add_untraceable_clause(IntVector& clits) { add_clause(clits); }

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
    return nullptr;
  }

protected:

  // Constructor/destructor

  SATSolverLowLevelNonIncrWrapper(IDManager& _imgr) : imgr(_imgr) {}

  virtual ~SATSolverLowLevelNonIncrWrapper(void) { }

protected:

  IDManager& imgr;

  IntVector model;

  BasicClauseVector ucore;

  LINT phase = 1;

  LINT verbosity = 0;

  bool inited = false;

  bool ready = false;

  LINT max_confls = -1;

  bool need_model = true;      // if false, do not fetch the model

  bool need_core = true;       // if false, do not fetch the core

};

/*----------------------------------------------------------------------------*/
