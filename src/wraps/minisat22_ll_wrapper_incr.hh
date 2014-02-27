//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        minisat22_ll_wrapper.hh
 *
 * Description: Low-level incremental wrapper for Minisat 2.2
 *
 * Author:      jpms
 * 
 *                                     Copyright (c) 2012, Joao Marques-Silva
\*----------------------------------------------------------------------------*/
//jpms:ec

#ifndef _MINISAT22_LL_WRAPPER_H
#define _MINISAT22_LL_WRAPPER_H 1

#include "vec.hh"
#include "solver_types.hh"
#include "solver.hh"
#include "simp_solver.hh"
#include "solver_ll_wrapper.hh"

//using namespace Minisat;

class SATSolverLLFactory;

//jpms:bc
/*----------------------------------------------------------------------------*\
 * Class: Minisat22LowLevelWrapperTmpl
 *
 * Purpose: Provides low-level incremental interface to Minisat22.
 *
 * Note: Template parameter can be instantiated with either Minisat::Solver or
 * Minisat::SimpSolver (and nothing else; if you need something else, add the
 * instantiation for that parameter at the end of .cc file). In the former case,
 * preprocess() is equivalent to runing BCP only. In the latter case, SatElite
 * is invoked (and disabled after the call).
\*----------------------------------------------------------------------------*/
//jpms:ec

template<class S>
class Minisat22LowLevelWrapperTmpl : public SATSolverLowLevelWrapper {
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

  void set_phase(ULINT var, LINT ph) {
    assert(var < nvars());
    solver->setPolarity(var, (ph<0)); // true argument means "false" in minisat
  }

  void set_random_seed(ULINT seed) {
    solver->random_seed = seed ? (double)seed/MAXULINT : 0.131008300313;
  }

  // Handle assumptions

  void set_assumption(ULINT svar, LINT sval) {
    DBG(cout<<"Setting assumption: " << svar << " w/ value: " << sval<<endl);
    assumps.push(Minisat::mkLit(svar, sval==0));
    DBG(cout<<"ASSUMP SIZE: "<<assumps.size()<<endl;);
  }

  void set_assumptions(IntVector& assumptions) {
    IntVector::iterator ipos = assumptions.begin();
    IntVector::iterator iend = assumptions.end();
    for(; ipos != iend; ++ipos) {
      set_assumption((ULINT) std::llabs(*ipos), (*ipos<0)?0:1);
    }
  }

  void clear_assumptions() { assumps.clear(); }


  // Add/remove clauses or clause sets

  void add_clause(ULINT svar, IntVector& litvect) {
    assert(clits.size() == 0);
    update_maxvid(svar);
    clits.push(Minisat::mkLit(svar, true));
    IntVector::iterator lpos = litvect.begin();
    IntVector::iterator lend = litvect.end();
    for(; lpos != lend; ++lpos) {
      LINT lit = *lpos;
      update_maxvid(std::llabs(lit));
      clits.push(Minisat::mkLit(std::llabs(lit), lit<0));
    }
    solver->addClause(clits);
    DBG(cout << "CL LITS: [";for(int i=0; i<clits.size(); ++i) { cout << toInt(clits[i]) << " "; } cout<<"0]\n";);
    clits.clear();
  }

  void add_clause(ULINT svar, BasicClause* cl) {
    assert(clits.size() == 0);
    update_maxvid(svar);
    clits.push(Minisat::mkLit(svar, true));
    Literator lpos = cl->begin();
    Literator lend = cl->end();
    for(; lpos != lend; ++lpos) {
      LINT lit = *lpos;
      update_maxvid(std::llabs(lit));
      clits.push(Minisat::mkLit(std::llabs(lit), lit<0));
    }
    solver->addClause(clits);
    DBG(cout << "CL LITS: [";for(int i=0; i<clits.size(); ++i) { cout << toInt(clits[i]) << " "; } cout<<"0]\n";);
    clits.clear();
  }

  void add_clauses(IntVector& svars, BasicClauseSet& rclset) {
    IntVector::iterator ipos = svars.begin();
    IntVector::iterator iend = svars.end();
    ClSetIterator cpos = rclset.begin();
    ClSetIterator cend = rclset.end();
    for(; cpos != cend; ++cpos, ++ipos) { add_clause(*ipos, *cpos); }
  }

  void add_final_clause(BasicClause* cl) {
    assert(clits.size() == 0);
    Literator lpos = cl->begin();
    Literator lend = cl->end();
    for(; lpos != lend; ++lpos) {
      LINT lit = *lpos;
      update_maxvid(std::llabs(lit));
      clits.push(Minisat::mkLit(std::llabs(lit), lit<0));
    }
    solver->addClause(clits);
    DBG(cout << "CL LITS: [";for(int i=0; i<clits.size(); ++i) { cout << toInt(clits[i]) << " "; } cout<<"0]\n";);
    clits.clear();
  }

  void add_final_clause(IntVector& litvect) {
    assert(clits.size() == 0);
    IntVector::iterator lpos = litvect.begin();
    IntVector::iterator lend = litvect.end();
    for(; lpos != lend; ++lpos) {
      LINT lit = *lpos;
      update_maxvid(std::llabs(lit));
      clits.push(Minisat::mkLit(std::llabs(lit), lit<0));
    }
    solver->addClause(clits);
    DBG(cout << "CL LITS: [";for(int i=0; i<clits.size(); ++i) { cout << toInt(clits[i]) << " "; } cout<<"0]\n";);
    clits.clear();
  }

  void del_clause(ULINT svar, IntVector& litvect) {
    assert(clits.size() == 0);
    clits.push(Minisat::mkLit(svar, true));
    solver->addClause(clits);
    clits.clear();
  }

  void del_clause(ULINT svar, BasicClause* cl=NULL) {
    clits.push(Minisat::mkLit(svar, true));
    solver->addClause(clits);
    clits.clear();
  }

  void del_clauses(IntVector& svars, BasicClauseSet& rclset) {
    IntVector::iterator ipos = svars.begin();
    IntVector::iterator iend = svars.end();
    ClSetIterator cpos = rclset.begin();
    ClSetIterator cend = rclset.end();
    for(; cpos != cend; ++cpos, ++ipos) { del_clause(*ipos, *cpos); }
  }

  void make_clause_final(ULINT svar, BasicClause* cl) {
    clits.push(Minisat::mkLit(svar, false));
    solver->addClause(clits);
    clits.clear();
  }

  // Preprocessing -- please read the comments in solver_ll_wrapper.hh !

  virtual bool is_preprocessing(void) { return simp; }

  SATRes preprocess(bool turn_off = false);

  void freeze_var(ULINT var);

  void unfreeze_var(ULINT var);

  void get_solver_clauses(BasicClauseSet& cset);

  // Printing/stats

  void print_cnf(const char* fname);

  // Additional info

  /** Returns the activity of the specified variable; -1 if variable doesn't
   * exist. */
  virtual double get_activity(ULINT var) {
    return (var < (ULINT)solver->nVars()) ? solver->varActivity(var) : -1;
  }

  // Direct access (see comments in the parent)

  virtual void* get_raw_solver_ptr(void) { return solver; }

protected:
//public:

  // Constructor/destructor

  Minisat22LowLevelWrapperTmpl(IDManager& _imgr);

  virtual ~Minisat22LowLevelWrapperTmpl();


protected:

  // Auxiliary functions

  inline void handle_sat_outcome();

  inline void handle_unsat_outcome();


protected:

  inline void update_maxvid(LINT nvid) {
    assert(nvid > 0);
    while(nvid >= solver->nVars()) {
      // take care of the phase: note polarity=true (first argument) in minisat
      // means phase=false ! if phase=0 (random), then the value doesn't matter;
      // phase=2 (default) maps to minisat's default, i.e. polarity=true
      solver->newVar((bool)(phase == -1), true);
    }
    assert(solver->nVars() > nvid);  // Always +1 var in solver
  }

  // Compute unsat core given map of assumptions to clauses
  void compute_unsat_core();

protected:

  S * solver;          // The actual solver being used

  bool simp;           // true when instantiated with simplifying solver

  Minisat::vec<Minisat::Lit> clits;

  Minisat::vec<Minisat::Lit> assumps;

};

/*----------------------------------------------------------------------------*\
 * Class: Minisat22LowLevelWrapper
 *
 * Purpose: Provides low-level incremental interface to Minisat22.
\*----------------------------------------------------------------------------*/
typedef Minisat22LowLevelWrapperTmpl<Minisat::Solver> Minisat22LowLevelWrapper;

/*----------------------------------------------------------------------------*\
 * Class: Minisat22sLowLevelWrapper
 *
 * Purpose: Provides low-level incremental interface to Minisat22 with SATElite
\*----------------------------------------------------------------------------*/
typedef Minisat22LowLevelWrapperTmpl<Minisat::SimpSolver> Minisat22sLowLevelWrapper;

#endif /* _MINISAT22_LL_WRAPPER_H */

/*----------------------------------------------------------------------------*/
