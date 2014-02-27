/*----------------------------------------------------------------------------*\
 * File:        minisat22_wrapper_gincr.hh
 *
 * Description: Incremental group-based wrapper for the MiniSAT 2.2 SAT solver.
 *
 * Author:      antonb
 *
 * Notes:       TODO -- generalize so anything can be plugged in.
 *
 *                          Copyright (c) 2011, Anton Belov, Joao Marques-Silva
\*----------------------------------------------------------------------------*/

#ifndef _MINISAT22_WRAPPER_GINCR_H
#define _MINISAT22_WRAPPER_GINCR_H 1

#include <ext/hash_set>
#include <iterator>
#include <set>
#include "globals.hh"
#include "solver_wrapper_gincr.hh"
#include "../minisat22-2/core/Solver.h"
#include "../minisat22-2/simp/SimpSolver.h"

//#define DBG(x) x

/*----------------------------------------------------------------------------*\
 * Class: Minisat22WrapperGrpIncrTmpl<Solver, simp>
 *
 * Purpose: Provides group-based incremental interface to the Minisat22 SAT
 * solver family. Template parameters:
 *   Solver -- the class name for Minisat-style solver.
 *   Preprocess -- if true, preprocessing is kicked off (Solver has to support
 *   it), otherwise no preprocesing. In either case, no on-the-fly simplification.
 *
\*----------------------------------------------------------------------------*/

template<typename Solver, bool Preprocess>
class Minisat22WrapperGrpIncrTmpl : public SATSolverWrapperGrpIncr {

  friend class SATSolverFactory;

protected: // Constructor/destructor (only for the factory)

  Minisat22WrapperGrpIncrTmpl(IDManager& _imgr) :
    SATSolverWrapperGrpIncr(_imgr), solver(new Solver()), 
    verbosity(0), phase(3) {
  }

  virtual ~Minisat22WrapperGrpIncrTmpl(void) {
    if (solver != NULL) {
      delete solver;
      solver = NULL;
    }
  }

public: // Implementation of the main interface methods

  /* Initialize all internal data structures */
  virtual void init_all(void) {
    SATSolverWrapperGrpIncr::init_all();
    if (solver != NULL) {
      delete solver;
      solver = NULL;
    }
    solver = new Solver();
    solver->verbosity = (verbosity > 3) ? (verbosity - 2) : 0;
    solver->rnd_pol = (phase == 2); // random polarity
  }

  /* Clean up all internal data structures */
  virtual void reset_all(void) {
    if (solver != NULL) {
      delete solver;
      solver = new Solver();
    }
    SATSolverWrapperGrpIncr::reset_all();
  }

  /* Call SAT solver
   */
  virtual SATRes solve(void) {
    if (!isvalid)
      throw std::logic_error("Solver interface is in invalid state.");
    if (Preprocess) {
      // eliminate(true) runs elimination, and turns it off so its not used
      // during solving. TODO: why not ???
      static_cast<Minisat::SimpSolver*>(solver)->eliminate(true);
    }
    // make a vector of assumptions
    Minisat::vec<Minisat::Lit> assums;
    for (GID2IntMap::iterator p = gid2a_map.begin(); p != gid2a_map.end(); ++p)
      if (p->second)
        assums.push(Minisat::mkLit(abs(p->second), p->second < 0));
    // add user assumptions
    for (LINT lit : user_assum) 
      assums.push(Minisat::mkLit(abs(lit), lit < 0));

    bool status = solver->solve(assums);

    if (verbosity > 2) {
      prt_std_cputime("c ", "Done running SAT solver ...");
    }

    if (status) {
      // SAT outcome -- get the model
      assert(model.size() == 0);
      model.resize(maxvid + 1, 0); // only store the real variables
      for (Minisat::Var var = 1; var <= (Minisat::Var)maxvid; ++var) {
        using Minisat::lbool; // b/c l_True/l_False are macros
        lbool v = solver->modelValue(var);
        model[var] = (v == l_True) ? 1 : ((v == l_False) ? -1 : 0);
      }
      return SAT_True;
    } else { 
      // UNSAT outcome -- get the core
      assert(gcore.size() == 0);
      // visit map from assumptions to groups, and for each group check if its
      // assumption literal is the final conflict clause of MiniSAT
      Minisat::vec<Minisat::Lit>& final_conf = solver->conflict;
      __gnu_cxx::hash_set<LINT, IntHash, IntEqual> conf_set;
      for (int i = 0; i < final_conf.size(); i++) {
        Minisat::Lit l = final_conf[i];
        // ignore user assumptions for now (TODO: add method to retrieve them)
        LINT lit = sign(l) ? -var(l) : var(l);
        if (user_assum.count(lit))
          continue;
        assert(lit > 0); // should be positive
        conf_set.insert(-lit);
      }
      for (GID2IntMap::iterator p = gid2a_map.begin(); p != gid2a_map.end(); ++p) {
        if (p->second && (conf_set.find(p->second) != conf_set.end())) {
          assert(p->second < 0);    // otherwise how can it be in failed assumption
          gcore.insert(p->first);
        }
      }
      return SAT_False;
    }
    return SAT_NoRes;
  }

  /* Solve the current set of clauses with extra assumptions;
   * assumptions are given as units in 'assum' and passed directly
   * to the solver without any modifications.
   */
  virtual SATRes solve(const IntVector& assum) {
    // make a local copy; solve; clear the copy
    copy(assum.begin(), assum.end(), inserter(user_assum, user_assum.begin()));
    SATRes res = solve();
    user_assum.clear();
    return res;
  }

public: // Configuration methods

  /* Verbosity -- usually passed to solver */
  virtual void set_verbosity(int verb) { verbosity = verb; }

  /* Sets the default phase: 0 - false, 1 - true, 2 - random */
  virtual void set_phase(LINT ph) { phase = (ph == 2) ? 3 : ph; }


public: // Implemented group interface

  /* Adds all groups in the groupset */
  virtual void add_groups(BasicGroupSet& gset, bool g0final = true) {
    // this is a bit of a hack -- if no preprocessing is asked for, but the
    // solver is simplifying, we want to disable the simplification; the way
    // to do it is to call eliminate(true) before any clauses are added --
    // this is the same way as in SimpSolver/Main.cc
    // TODO: figure out a better way (check for template parameter type)
    if (!Preprocess) {
      Minisat::SimpSolver* ssolver = dynamic_cast<Minisat::SimpSolver*>(solver);
      if (ssolver)
        ssolver->eliminate(true); // true disables simplifications in the future
    }
    SATSolverWrapperGrpIncr::add_groups(gset, g0final);
  }

protected: // Solver-specific methods

  /* Add clause to minisat22, with associated assumption (0 means no assumption)
   */
  virtual void solver_add_clause(BasicClause* cl, LINT alit = 0) 
  {
    Minisat::vec<Minisat::Lit> lits; // TODO: resize right away
    for (Literator lpos = cl->abegin(); lpos != cl->aend(); ++lpos) 
      {
        Minisat::Var var = minisat_add_var(abs(*lpos));
        lits.push(Minisat::mkLit(var, *lpos < 0));
        update_maxvid((ULINT)var); // only for "real" clauses
      }
    if (alit) 
      {
        Minisat::Var var = minisat_add_var(abs(alit));
        lits.push(Minisat::mkLit(var, false)); // assumption literals are positive
        if (Preprocess) // then assumption literals are frozen 
          static_cast<Minisat::SimpSolver*>(solver)->setFrozen(var, true);
      }
    DBG(std::cout << "Added clause: "; cl->dump();
        std::cout << ", assumpt: " << alit << std::endl;);
    solver->addClause(lits);
  }

  /**
     Add unit clause, but without any checks - i.e. assumes that the literal
     already exists (this is used for finalizing clauses)
   */
  virtual void solver_assert_unit_clause(LINT lit) 
  {
    solver->addClause(lit > 0 ? Minisat::mkLit(lit) : ~Minisat::mkLit(-lit));
    DBG(std::cout << "Asserted unit: " << lit << std::endl;);
  }

  /**
     Ensures minisat has enough variables; returns the variable (for convenience)
   */
  Minisat::Var minisat_add_var(Minisat::Var var) 
  {
    while (var >= solver->nVars()) 
      {
        // take care of the phase: note polarity=true (first argument) in minisat
        // means phase=0 ! if phase=2, then the value doesn't matter; phase=3 (default)
        // maps to minisat's default, i.e. polarity=true
        solver->newVar((bool)(phase != 1), true);
      }
    return var;
  }// minisat_add_var

  /**
     Ensures minisat has enough variables; returns the variable (for convenience)
  */
  inline void initMyMinisat(int m) 
  {
    solver->setMaxIndexVarInit(m);    
  }// initShiftWithNvar

  /**
     Show the information of the CDCL solver
  */
  inline void showInfoCDCL()
  {
    solver->showInfoCDCL();
  }


protected:

  Solver* solver;                   // The actual solver being used
  int verbosity;                    // verbosity
  int phase;                        // phase
  set<LINT> user_assum;             // assumptions passed by the user
};


// Typedefs for useful instantiations

/* The regular solver */
typedef Minisat22WrapperGrpIncrTmpl<Minisat::Solver, false> Minisat22WrapperGrpIncr;

/* The solver with preprocessing */
typedef Minisat22WrapperGrpIncrTmpl<Minisat::SimpSolver, true> Minisat22SWrapperGrpIncr;

#endif /* _MINISAT22_WRAPPER_GINCR_H */

/*----------------------------------------------------------------------------*/
