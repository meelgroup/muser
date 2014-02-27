//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        solver_sls_wrapper.hh
 *
 * Description: SAT solver wrapper for SLS-based solvers
 *
 * Author:      antonb
 * 
 *                                     Copyright (c) 2012, Anton Belov
\*----------------------------------------------------------------------------*/
//jpms:ec

#ifndef _SOLVER_SLS_WRAPPER_H
#define _SOLVER_SLS_WRAPPER_H 1

#include <stdexcept>
#include "basic_clause.hh"
#include "basic_clset.hh"
#include "globals.hh"
#include "id_manager.hh"
#include "solver_utils.hh"

class SATSolverSLSFactory;

//jpms:bc
/*----------------------------------------------------------------------------*\
 * Class: SATSolverSLSWrapper
 *
 * Purpose: provides interface to SLS-based SAT solver
\*----------------------------------------------------------------------------*/
//jpms:ec

class SATSolverSLSWrapper {

  friend class SATSolverSLSFactory;

public:  // Lifecycle

  /** Initializes all internal data structs; call once per life-time */
  virtual void init_all(void) = 0;

  /** Inverse of init_all(), call before destruction */
  virtual void reset_all(void) = 0;

  /** Prepares for SAT run; call before each solve() */
  virtual void init_run(void) = 0;

  /** Inverse of init_run(), call after solve() */
  virtual void reset_run(void) = 0;

public:  // Functionality

  /** Find an assignment or an approximation, starting from random. The meaning
   * of the returned value is as follows:
   *  SAT_True    -- the solution quality is achieved (i.e. SAT if target is 0)
   *  SAT_False   -- the solution quality is provably unachievable (e.g. 
   *                 determined UNSAT)
   *  SAT_Unknown -- the solution quality is not achieved, but if there is a 
   *                 solution with the quality not worse than the initial one, 
   *                 it will be available. Whether such solution is available 
   *                 can be checked by calling has_decent_assignment()
   */
  virtual SATRes solve(void) = 0;

  /** Find an assignment or an approximation, starting from init_assign.
   * Unassigned variables are initialized randomly. 
   */
  virtual SATRes solve(const IntVector& init_assign) = 0;

public:   // Configuration; some methods are virtual to support partial 
          // implementations

  /** Solver-specific verbosity level (may be ignored) */
  void set_verbosity(int verbosity_) { verbosity = verbosity_; }

  /** Weighted or not (do not change between runs) */
  virtual void set_weighted(bool weighted_) { 
    ensure_state(CREATED); weighted = weighted_; }

  /** Use walksat algorithm, with the specified walk probability
   * (do not change the algorithm between runs) 
   */
  virtual void set_algo_walksat_skc(float wp_ = 0.3) {
    ensure_state(CREATED); algo = WALKSAT_SKC; wp = wp_; }

  /** Use adaptivenovelty+ algorithm, with the specified walk probability
   * (do not change between runs) 
   */
  virtual void set_algo_adaptnovelty_plus(float wp_ = 0.01) {
    ensure_state(CREATED); algo = ADAPTNOVELTY_PLUS; wp = wp_; }

  /** Use the "Captain Jack" algorithm [Tompkins, Balint, Hoos [SAT 11]]
   */
  virtual void set_algo_captain_jack(void) {
    ensure_state(CREATED); algo = CAPTAIN_JACK; }

  /** Set target quality for the following runs. 0 by default. */
  virtual void set_target_quality(XLINT trgt_quality_) { 
    ensure_state((State)(CREATED | INITIALIZED)); trgt_quality = trgt_quality_; }

  /** Set maximum number of tries per solve() for the following runs. 1 by default */
  virtual void set_max_tries(ULINT tries_) { 
    ensure_state((State)(CREATED | INITIALIZED)); tries = tries_; }

  /** Set maximum number of steps per try for the folowing runs. 1e06 by default */
  virtual void set_cutoff(ULINT cutoff_) { 
    ensure_state((State)(CREATED | INITIALIZED)); cutoff = cutoff_; }

  /** Set the timeout (sec) per solve() for the following runs. 0 means no time 
   * out (default) 
   */
  virtual void set_timeout(float timeout_) {
    ensure_state((State)(CREATED | INITIALIZED)); timeout = timeout_; }

  /** Set the limit in the number steps per try with no improvement in quality. 
   * If not 0 (default 0), a try will be terminated if there is no improvement in solution 
   * quality for this number of steps.
   */
  virtual void set_noimprove(ULINT noimprove_) {
    ensure_state((State)(CREATED | INITIALIZED)); noimprove = noimprove_; }

  /** Set the ceiling on the quality of the worstening step during SLS: if the
   * current solution quality is < max_break_value, but a selected flip would 
   * cause the solution quality to become >= max_break_value, the flip is 
   * aborted. The value of 0 disables this functionality.
   */
  virtual void set_max_break_value(XLINT max_break_value_) {
    ensure_state((State)(CREATED | INITIALIZED)); max_break_value = max_break_value_; }

public:  // Access result of SAT solver call

  /** Returns the quality of initial assignment (either random or passed in solve())
   */
  virtual XLINT get_init_quality(void) const {
    ensure_state(SOLVED); return init_quality;
  }

  /** Returns the quality of the final assignment (i.e. the last assignment before
   * solve() terminated this run.
   */
  virtual XLINT get_final_quality(void) const {
    ensure_state(SOLVED); return final_quality;
  }

  /** Returns a read-only reference to a satistying assignment in case
   * solve() returned SAT_True, or an approximation in case of SAT_Unknown
   * and get_final_quality() < get_init_quality()
   */
  virtual const IntVector& get_assignment(void) const {
    ensure_state(SOLVED); return assignment; }

  /** Makes a copy of get_assignment(void) into ass (may shrink/enlarge ass) ;-) */
  virtual void get_assignment(IntVector& ass) const {
    ensure_state(SOLVED); ass = assignment;
  }

public:  // Manipulate local copy of clause set

  /** Returns the number of clauses in the solver */
  virtual LINT size(void) const = 0;

  /** Returns the maximum variable id in the solver */
  virtual ULINT max_var(void) const = 0;

  /** Adds a clause to the solver. If the solver is weighted the specified weight
   * is used, which must be > 0.
   */
  virtual void add_clause(const BasicClause* cl, XLINT weight) = 0;

  /** Adds a clause to the solver. If the solver is weighted the clause weight
   * is used (cl->get_weight()). Can be called between runs.
   */
  virtual void add_clause(const BasicClause* cl) = 0;

  /** Same as above, but with a vector of literals, and weight.
   */
  virtual void add_clause(const IntVector& lits, XLINT weight) = 0;

  /** Adds all clauses from the clause set. If the solver is weighted the weight
   * is taken from the clauses. Can be called between runs.
   */
  virtual void add_clauses(const BasicClauseSet& rclset) = 0;

  /** Notifies the wrapper of a change in the weight of the specified clause; the
   * clause is identified by its ID (get_id()); returns true if weight is updated
   * successfully.
   */
  virtual bool update_clause_weight(const BasicClause* cl) = 0;


public:  // Stats

  /** Returns the total number of runs executed since the init_all() call.
   */
  ULINT get_num_runs(void) const { return num_runs; }

  /** Returns the total number of successfull runs since the init_all() call.
   */
  ULINT get_num_solved(void) const { return num_solved; }

  /** Returns the number of runs where the solution quality was improved.
   */
  ULINT get_num_improved(void) const { return num_improved; }

protected:  // Lifecycle

  enum State {  // wrapper state
    CREATED = 0x1,                      // created (but not initialized)
    INITIALIZED = 0x2,                  // after init_all()
    PREPARED = 0x4,                     // prepared for run (after init_run())
    SOLVED = 0x8                        // run is finished (can get results)
  };

  SATSolverSLSWrapper(IDManager& _imgr) :
    imgr(_imgr), state(CREATED), init_quality(0), final_quality(0), 
    algo(WALKSAT_SKC), verbosity(0), weighted(false), trgt_quality(0), 
    tries(1), cutoff(1000000), timeout(0.0f), noimprove(0), wp(0),
    max_break_value(0), num_runs(0), num_solved(0), num_improved(0) {}

  virtual ~SATSolverSLSWrapper(void) {}

  void set_state(State st) { state = st; }

  void ensure_state(State st) const {
    if (!(state&st))
      throw std::logic_error("SLS wrapper is in invalid state: " +
            convert<int>(state) + " instead of " + convert<int>(st));
  }

protected:  // Data/state

  IDManager& imgr;

  State state;

  IntVector assignment;                 // keeps the assignment

  XLINT init_quality;                   // intial assignment quality this run

  XLINT final_quality;                  // final assignment quality this run

protected:  // Configuration

  enum SLSAlgorithm {                   // Enumeration for SLS algorithms
    WALKSAT_SKC,                        // walksat SKC
    ADAPTNOVELTY_PLUS,                  // adaptive novelty+
    CAPTAIN_JACK                        // Captain Jack
  };

  SLSAlgorithm algo;

  LINT verbosity;

  bool weighted;        // weighted or not
  
  XLINT trgt_quality;   // target solution quality

  ULINT tries;          // max. number of tries

  ULINT cutoff;         // cutoff per try

  float timeout;        // timeout (sec) for solve()

  ULINT noimprove;      // max steps without improvement

  float wp;             // random walk probability (e.g. for walksat)

  XLINT max_break_value;// max. break value (see set_max_break_value())

protected:  // Stats

  ULINT num_runs;       // total number of runs executed

  ULINT num_solved;     // number of successfull runs

  ULINT num_improved;   // number of runs with improved quality

};


#endif /* _SOLVER_SLS_WRAPPER_H */

/*----------------------------------------------------------------------------*/
