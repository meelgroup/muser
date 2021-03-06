//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        solver_wrapper_gincr.hh
 *
 * Description: An adapter class that provides an incremental group-based
 *              interface to an instance of SATSolverLowLevelWrapper.
 *
 * Author:      antonb
 *
 * Notes:
 *
 *                                              Copyright (c) 2013, Anton Belov
\*----------------------------------------------------------------------------*/
//jpms:ec

#pragma once

#include <unordered_map>
#include "solver_wrapper.hh"
#include "solver_ll_wrapper.hh"

/*----------------------------------------------------------------------------*\
 * Class: SATSolverWrapperGrpIncr
 *
 * Purpose: A group-based wrapper around SATSolverLowLevelWrapper.
 *
\*----------------------------------------------------------------------------*/

class SATSolverWrapperGrpIncr : public MUSer2::SATSolverWrapper {

  friend class MUSer2::SATSolverFactory;

protected:  // Constructor/destructor

  SATSolverWrapperGrpIncr(IDManager& _imgr, SATSolverLowLevelWrapper& _llwrap)
    : MUSer2::SATSolverWrapper(_imgr), llwrap(_llwrap) {}
  
  virtual ~SATSolverWrapperGrpIncr(void) {}

public: // Implementation of the main interface methods

  /* Initialize all internal data structures */
  virtual void init_all(void) override;

  /* Clean up all internal data structures */
  virtual void reset_all(void) override;

  /* Initialize data structures for SAT run */
  virtual void init_run(void) override;

  /* Clean up data structures from SAT run */
  virtual void reset_run(void) override;

  /* Solve the current set of clauses instance */
  virtual SATRes solve(void) override { return solve(nullptr); }

  /* Solve the current set of clauses with extra assumptions;
   * assumptions are given as units in 'assum' and passed directly
   * to the solver without any modifications.
   */
  virtual SATRes solve(const IntVector& assum) override { return solve(&assum); }

public: // Additional non-mandatory functionality

  /** Returns true if this solver knows to do preprocessing; if this is overriden,
   * then preprocess() should be implemented.
   */
  virtual bool is_preprocessing(void) override { return llwrap.is_preprocessing(); }

  /** Preprocess the current set of clauses. If turn_off is set to true, no
   * further preprocessing is possible; this might allow some solvers to release
   * resources.
   * @return SAT_True, SAT_False, SAT_NoRes depending on the results of prepro.
   */
  virtual SATRes preprocess(bool turn_off = false) override { return llwrap.preprocess(turn_off); }

  /** Returns the activity of the specified variable
   */
  virtual double get_activity(ULINT var) override { return llwrap.get_activity(var); }

public: // Configuration methods

  /* Verbosity -- usually passed to solver */
  virtual void set_verbosity(int verb) override { llwrap.set_verbosity(verb); }

  /* Sets the default phase: 0 - false, 1 - true, 2 - random */
  virtual void set_phase(LINT phase) override { llwrap.set_def_phase(phase); }

  /* If the solver supports this, sets the output stream to be used for writing
   * out the proof trace in case of UNSAT outcome at the end of solve(); it
   * is up to called to open/close the stream; 0 disables writing
   */
  virtual void set_proof_trace_stream(FILE* o_stream) override {
    llwrap.set_proof_trace_stream(o_stream);
  }

  /* Sets the preferred phase for a particular variable: 0 - false, 1 - true
   */
  virtual void set_phase(ULINT var, LINT phase) override {
    llwrap.set_polarity(var, (bool)phase);
  }

  /* Sets the maximum number of conflicts per call. Note that this affects
   * completeness. -1 = no maximum.
   */
  virtual void set_max_conflicts(LINT max_conflicts) override {
    llwrap.set_max_conflicts(max_conflicts);
  }

  /** Some incremental solvers implement optimizations that require the knowledge
   * of which variables are selectors. This method allows to set the largest
   * variable ID of problem variables (i.e. everything above that is a selector)
   */
  virtual void set_max_problem_var(ULINT pvar) override {
    llwrap.set_max_problem_var(pvar);
  }

public:   // Access result of SAT solver call

  /* Returns the reference to the model (r/w) */
  virtual IntVector& get_model(void) override { return llwrap.get_model(); }

  /* Makes a copy of the model (resized as needed) */
  virtual void get_model(IntVector& rmodel) override { llwrap.get_model(rmodel);}

  /* Returns the reference to the group unsat core */
  virtual GIDSet& get_group_unsat_core(void) override { return gcore; }

public: // Implemented non-group interface

  /* This assumes that the GID of the clause is set correctly. It will create
   * a new assumption variable if the GID has not been yet seen. This method 
   * will  not check if the clause is marked as removed. Group 0 clauses are 
   * added as final.
   */
  virtual void add_clause(BasicClause* cl) override;

  /* Adds a final clause (non-removable) */
  virtual void add_final_clause(BasicClause* cl) override;

  /* Adds a final unit clause (non-removable) */
  virtual void add_final_unit_clause(LINT lit) override;

public: // Implemented group interface

  /* Number of groups (including 0) */
  virtual LINT gsize(void) override { return g2a_map.size() + has_g0; }

  /* Maximum GID ever used in the solver */
  virtual GID max_gid(void) override { return maxgid; }

  /* Adds all groups in the groupset */
  virtual void add_groups(BasicGroupSet& gset, bool g0final = true) override;

  /* Adds a single group from the groupset; if final = true the group is
   * added as final right away.
   */
  virtual void add_group(BasicGroupSet& gset, GID gid, bool final = false) override;

  /* True if group exists in the solver */
  virtual bool exists_group(GID gid) override { return g2a_map.count(gid); }

  /* Activates (non-final) group */
  virtual void activate_group(GID gid) override;

  /* Deactivates (non-final) group */
  virtual void deactivate_group(GID gid) override;

  /* Returns true if either final, or non-final and active */
  virtual bool is_group_active(GID gid) override;

  /* Removes (non-final) group */
  virtual void del_group(GID gid) override;

  /* Finalizes a group */
  virtual void make_group_final(GID gid) override;

  /* True if group is final */
  virtual bool is_group_final(GID gid) override;

  /* Returns the activation literal for group -- setting to true makes the
   * group inactive; 0 means the group has been finalized. */
  virtual LINT get_group_activation_lit(GID gid) override;

public:  // Miscellaneous (stats, printing, etc)

  /** Access to the underlying SAT solver.
   */
  virtual void* get_raw_solver_ptr(void) override {
    return llwrap.get_raw_solver_ptr();
  }

protected: // helpers

  /* Runs the solve() method and processes the results */
  SATRes solve(const IntVector* assump);

  /* Add a clause to solver, with associated assumption (0 means no assumption)
   */
  void solver_add_clause(BasicClause* cl, ULINT alit = 0);

  /* Add (assert) a unit clause */
  void solver_assert_unit_clause(LINT lit);

  void update_maxgid(GID gid) { if (gid > maxgid) { maxgid = gid; } }
  
protected:

  bool isvalid = false;

  SATSolverLowLevelWrapper& llwrap;  // low-level incremental wrapper

  std::unordered_map<GID,LINT> g2a_map; // map from group IDs to assumptions;
                                    // 0 means the group is final
                                    // <0 means the group is active
                                    // >0 means the group is inactive

  std::unordered_map<ULINT,GID> a2g_map; // map from assumptions to GID

  GIDSet gcore;                     // group core

  GID maxgid = 0;                   // maximum group id (ever used)

  bool has_g0 = false;              // true when g0 has been added as final
                                    // (this is not good; fix)

private:

  IntVector _clits;                 // vector for literals

};

/*----------------------------------------------------------------------------*/
