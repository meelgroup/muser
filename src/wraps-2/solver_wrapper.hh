//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        solver_wrapper.hh
 *
 * Description: General SAT solver wrapper & SAT solver factory.
 *
 * Author:      jpms
 * 
 * Modifications:
 *
 *    antonb - fixed and added group-based interface methods; rewrote completely.
 *
 * Notes:
 *    (1) the default implementation of most of the methods just throws exceptions;
 *    this is done because the actual wrappers might differ significantly in the
 *    set of supported functionality.
 *
 *    (2) the class is wrapped into MUSer2 namespace to avoid conflicts with
 *    wraps/. I think this needs to be completely re-designed.
 *
 *                        Copyright (c) 2010-13, Joao Marques-Silva, Anton Belov
\*----------------------------------------------------------------------------*/
//jpms:ec

#pragma once

#include <stdexcept>
#include "basic_clset.hh"
#include "basic_group_set.hh"
#include "id_manager.hh"
#include "solver_utils.hh"

namespace MUSer2 {

class SATSolverFactory;

//jpms:bc
/*----------------------------------------------------------------------------*\
 * Class: SATSolverWrapper
 *
 * Purpose: Provides configurable interface to SAT solver
\*----------------------------------------------------------------------------*/
//jpms:ec

class SATSolverWrapper {

  friend class SATSolverFactory;

public: // The main interface methods -- this is the minimal set of methods that
        // must be implemented by concrete subclasses.

  /* Initialize all internal data structures */
  virtual void init_all(void) = 0;      

  /* Clean up all internal data structures */
  virtual void reset_all(void) = 0;     

  /* Initialize data structures for SAT run */
  virtual void init_run(void) = 0;      

  /* Clean up data structures from SAT run */
  virtual void reset_run(void) = 0;     

  /* Solve the current set of clauses instance */
  virtual SATRes solve(void) = 0;       

  /* Solve the current set of clauses with extra assumptions;
   * assumptions are given as units in 'assum' and passed directly
   * to the solver without any modifications.
   */
  virtual SATRes solve(const IntVector& assum) = 0;

public: // Additional non-mandatory functionality

  /** Returns true if this solver knows to do preprocessing; if this is overriden,
   * then preprocess() should be implemented.
   */
  virtual bool is_preprocessing(void) { return false; }

  /** Preprocess the current set of clauses. If turn_off is set to true, no
   * further preprocessing is possible; this might allow some solvers to release
   * resources.
   * @return SAT_True, SAT_False, SAT_NoRes depending on the results of prepro.
   */
  virtual SATRes preprocess(bool turn_off = false) {
    tool_abort("preprocess() is not implemented for this solver.");
    return SAT_NoRes;
  }

  /** Returns the activity of the specified variable
   */
  virtual double get_activity(ULINT var) {
    throw std::logic_error("method is not implemented");
  }

public: // Configuration methods

  /* Verbosity -- usually passed to solver */
  virtual void set_verbosity(int verb) {   
    throw std::logic_error("method is not implemented");
  }

  /* Sets the default phase: 0 - false, 1 - true, 2 - random */
  virtual void set_phase(LINT phase) {   
    throw std::logic_error("method is not implemented");
  }

  /* If the solver supports this, sets the output stream to be used for writing
   * out the proof trace in case of UNSAT outcome at the end of solve(); it
   * is up to called to open/close the stream; 0 disables writing 
   */
  virtual void set_proof_trace_stream(FILE* o_stream) {   
    throw std::logic_error("method is not implemented");
  }     

  /* Sets the preferred phase for a particular variable: 0 - false, 1 - true
   */
  virtual void set_phase(ULINT var, LINT phase) {
    throw std::logic_error("method is not implemented");
  }

  /* Sets the maximum number of conflicts per call. Note that this affects
   * completeness. -1 = no maximum.
   */
  virtual void set_max_conflicts(LINT max_conflicts) {
    throw std::logic_error("method is not implemented");
  }

  /* Sets the timeout per call in seconds. 0 = no timeout
   */
  virtual void set_timeout(float to) {
    throw std::logic_error("method is not implemented");
  }

  /** Some incremental solvers implement optimizations that require the knowledge
   * of which variables are selectors. This method allows to set the largest
   * variable ID of problem variables (i.e. everything above that is a selector)
   */
  virtual void set_max_problem_var(ULINT pvar) { }

public:   // Access result of SAT solver call

  /* Returns the reference to the model (r/w) */
  virtual IntVector& get_model(void) = 0;

  /* Makes a copy of the model (resized as needed) */
  virtual void get_model(IntVector& rmodel) = 0;

  /* Returns the reference to a clausal unsat core */
  virtual BasicClauseVector& get_unsat_core(void) {
    throw std::logic_error("method is not implemented");
  }

  /* Returns the reference to the group unsat core */
  virtual GIDSet& get_group_unsat_core(void) {
    throw std::logic_error("method is not implemented");
  }


public: // Manipulate local copy of clause set on a clause-by-clause basis;
        // most of the wrappers will support either group or clausal interface

  /* Returns the number of clauses in the solver */
  virtual LINT size(void) {
    throw std::logic_error("method is not implemented");
  }
  /* Adds a clause -- the clause can be removed later */
  virtual void add_clause(BasicClause* cl) {
    throw std::logic_error("method is not implemented");
  }
  /* Adds a set of clauses -- the clauses can be removed later */
  virtual void add_clauses(BasicClauseSet& rclset) {
    throw std::logic_error("method is not implemented");
  }
  /* Checks if the clause is in the solver */
  virtual bool exists_clause(BasicClause* cl) {
    throw std::logic_error("method is not implemented");
  }
  /* ??? */
  virtual void replace_clause(BasicClause* cl) {
    throw std::logic_error("method is not implemented");
  }
  /* Removes a (non-final) clause */
  virtual void del_clause(BasicClause* cl) {
    throw std::logic_error("method is not implemented");
  }
  /* Removes all (non-final) clauses */
  virtual void del_all_clauses() {
    throw std::logic_error("method is not implemented");
  }
  /* Enables a (non-final) clause */
  virtual void activate_clause(BasicClause* cl) {
    throw std::logic_error("method is not implemented");
  }
  /* Disables a (non-final) clause */
  virtual void deactivate_clause(BasicClause* cl) {
    throw std::logic_error("method is not implemented");
  }
  /* Enables all (non-final) clauses */
  virtual void activate_all_clauses() {
    throw std::logic_error("method is not implemented");
  }
  /* Disables all (non-final) clauses */
  virtual void deactivate_all_clauses() {
    throw std::logic_error("method is not implemented");
  }
  /* Adds a final clause */
  virtual void add_final_clause(BasicClause* cl) {
    throw std::logic_error("method is not implemented");
  }
  /* Adds a final unit clause */
  virtual void add_final_unit_clause(LINT lit) { 
    throw std::logic_error("method is not implemented");
  } 
  /* Add all clauses as final */
  virtual void add_final_clauses(BasicClauseSet& rclset) {
    throw std::logic_error("method is not implemented");
  }
  /* Makes a clause final (non-removable) */
  virtual void make_clause_final(BasicClause* cl) {
    throw std::logic_error("method is not implemented");
  }

public: // Manipulate local copy of clause set on a group basis; most of the wrappers 
        // will support either group or clausal interface

  /* Number of groups (including 0) */
  virtual LINT gsize(void) {    
    throw std::logic_error("method is not implemented");
  }
  /* Maximum GID ever used in the solver */
  virtual GID max_gid(void) {   
    throw std::logic_error("method is not implemented");
  }
  /* Adds all groups in the groupset */
  virtual void add_groups(BasicGroupSet& gset, bool g0final = true) {
    throw std::logic_error("method is not implemented");
  }
  /* Adds a single group from the groupset; if final = true the group is
   * added as final right away.
   */
  virtual void add_group(BasicGroupSet& gset, GID gid, bool final = false) {
    throw std::logic_error("method is not implemented");
  }
  /* True is group exists in the solver */
  virtual bool exists_group(GID gid) {
    throw std::logic_error("method is not implemented");
  }
  /* Activates (non-final) group */
  virtual void activate_group(GID gid) {
    throw std::logic_error("method is not implemented");
  }
  /* Deactivates (non-final) group */
  virtual void deactivate_group(GID gid) {
    throw std::logic_error("method is not implemented");
  }
  /* Returns true if either final, or non-final and active */
  virtual bool is_group_active(GID gid) {
    throw std::logic_error("method is not implemented");
  }
  /* Removes (non-final) group */
  virtual void del_group(GID gid) {
    throw std::logic_error("method is not implemented");
  }
  /* Finalizes a group */
  virtual void make_group_final(GID gid) {
    throw std::logic_error("method is not implemented");
  }
  /* Returns the activation literal for group -- setting to true makes the
   * group inactive; 0 means the group has been finalized. 
   */
  virtual LINT get_group_activation_lit(GID gid) {
    throw std::logic_error("method is not implemented");
  }
  /* True if group is final */
  virtual bool is_group_final(GID gid) {
    throw std::logic_error("method is not implemented");
  }

public:  // Miscellaneous (stats, printing, etc)

  /* Prints CNF into the specified output stream */
  virtual void print_cnf(FILE* o_stream) {
    throw std::logic_error("method is not implemented");
  }

  /** This method can be implemented to give access to the underlying SAT
   * solver instance. This may be useful to tweak some solver-specific low-level
   * configuation parameters.
   */
  virtual void* get_raw_solver_ptr(void) {
    throw std::logic_error("method is not implemented");
  }

protected: // Constructor/destructor -- to be used from factory only

  SATSolverWrapper(IDManager& _imgr) : imgr(_imgr) {}

  virtual ~SATSolverWrapper(void) {}

protected:

  IDManager& imgr;                      // id manager

};

} // namespace MUSer2

/*----------------------------------------------------------------------------*/
