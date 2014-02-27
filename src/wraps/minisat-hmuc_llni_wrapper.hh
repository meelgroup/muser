//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        minisat-hmuc_llni_wrapper.hh
 *
 * Description: Low-level wrapper for MinisatHMUC
 *
 * Author:      antonb
 * 
 *                                              Copyright (c) 2013, Anton Belov
\*----------------------------------------------------------------------------*/
//jpms:ec

#pragma once

// full paths required to make sure the correct versions are picked up
#include "../extsrc/minisat-hmuc/mtl/Vec.h"
#include "../extsrc/minisat-hmuc/core/SolverTypes.h"
#include "../extsrc/minisat-hmuc/core/Solver.h"
#include "../extsrc/minisat-hmuc/simp/SimpSolver.h"
#include "solver_llni_wrapper.hh"

class SATSolverLLNIFactory;

//jpms:bc
/*----------------------------------------------------------------------------*\
 * Class: MinisatHMUCLowLevelNonIncrWrapper
 *
 * Purpose: Provides low-level non-incremental interface to MinisatHMUC
 *
 * Note: the wrapper allows to add clauses *only* between init_solver() and
 * init_run().
 *
\*----------------------------------------------------------------------------*/
//jpms:ec

class MinisatHMUCLowLevelNonIncrWrapper : public SATSolverLowLevelNonIncrWrapper {
  friend class SATSolverLLNIFactory;

public:

  // Direct interface (see base for comments)

  virtual void init_solver(void);

  virtual void init_run(void);

  virtual SATRes solve(void);

  virtual void reset_run(void);

  virtual void reset_solver(void);

  virtual ULINT nvars(void) { return (ULINT) solver->nVars(); }

  virtual ULINT ncls(void) { return (ULINT) (solver->nClauses()+solver->nLearnts()); }


  // Config

  virtual void set_phase(ULINT var, LINT ph) {
    assert(var < nvars());
    solver->setPolarity(var, (ph<0)); // "true" polarity means 0 in minisat
  }

  virtual void set_max_conflicts(LINT mconf) {
    tool_abort("set_max_conflicts() is not implemented for this solver.");
  }

  virtual void set_random_seed(ULINT seed) {
    solver->random_seed = seed ? (double)seed/MAXULINT : 0.131008300313;
  }

  // Add/remove clauses or clause sets (see Note above)

  virtual void add_clause(BasicClause* cl) { _add_clause(cl); }

  virtual void add_clause(IntVector& clits) { _add_clause(clits.begin(), clits.end()); }

  virtual void add_clauses(BasicClauseSet& rclset) {
    for (BasicClause* cl : rclset) { _add_clause(cl); }
  }

  // Preprocessing -- please read the comments in solver_llni_wrapper.hh !

  virtual bool is_preprocessing(void) { return true; }

  virtual SATRes preprocess(bool turn_off = false);

  virtual void freeze_var(ULINT var) { solver->setFrozen(var, true); }

  virtual void unfreeze_var(ULINT var) { solver->setFrozen(var, false); }

  // Direct access (see comments in the parent)

  virtual void* get_raw_solver_ptr(void) { return solver; }

protected:

  // Constructor/destructor

  MinisatHMUCLowLevelNonIncrWrapper(IDManager& _imgr);

  virtual ~MinisatHMUCLowLevelNonIncrWrapper(void);

protected:

  // Auxiliary functions

  int _add_clause(BasicClause* cl);
  int _add_clause(Literator pbegin, Literator pend);

  void handle_sat_outcome(void);

  void handle_unsat_outcome(void);

protected:

  MinisatHMUC::SimpSolver * solver = nullptr; // The actual solver being used

  bool ic_confl = false;                      // when true, added clauses give
                                              // conflict by unit prop
  MinisatHMUC::vec<MinisatHMUC::Lit> clits;

  std::vector<BasicClause*> uid2cl;          // map: index is uid of a clause

};

/*----------------------------------------------------------------------------*/
