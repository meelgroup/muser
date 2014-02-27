//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        minisat-hmuc_ll_wrapper.cc
 *
 * Description: implementation of low-level incremental wrapper for minisat-abbr
 *
 * Author:      antonb
 *
 *                                              Copyright (c) 2013, Anton Belov
\*----------------------------------------------------------------------------*/
//jpms:ec

#include "globals.hh"
#include "minisat-abbr_ll_wrapper.hh"

using namespace MinisatAbbr;

//#define DBG(x) x

// Constructor.
template<class S>
MinisatAbbrLowLevelWrapperTmpl<S>::MinisatAbbrLowLevelWrapperTmpl(IDManager& _imgr) :
  SATSolverLowLevelWrapper(_imgr)
{
  tool_warn("minisat-abbr needs to know about selectors and assumes <= 1 selector per clause");
  solver = new S();
  simp = (dynamic_cast<SimpSolver*>(solver) != NULL);
}

template<class S>
MinisatAbbrLowLevelWrapperTmpl<S>::~MinisatAbbrLowLevelWrapperTmpl(void)
{
  if (solver != NULL) { delete solver; solver = nullptr; }
}

template<class S>
void MinisatAbbrLowLevelWrapperTmpl<S>::init_run(void)
{
  assert(!isvalid);
  model.clear(); ucore.clear(); isvalid = true;
  solver->verbosity = (verbosity > 20) ? 5 : 0;
  solver->rnd_pol = (phase == 0);
}

template<class S>
void MinisatAbbrLowLevelWrapperTmpl<S>::reset_run(void)
{
  assert(isvalid);
  assumps.clear(); model.clear(); ucore.clear(); isvalid = false;
  solver->verbosity = 0;
}

template<class S>
void MinisatAbbrLowLevelWrapperTmpl<S>::reset_solver(void)
{
  DBG(cout << "[minisat-abbr] reset solver" << endl;);
  if (solver != nullptr) {
    delete solver;
    solver = new S();
    solver->verbosity = (verbosity > 5) ? 5 : 0;
    solver->rnd_pol = (phase == 0);
  }
  assumps.clear(); model.clear(); ucore.clear(); isvalid = false;
}

template<class S>
SATRes MinisatAbbrLowLevelWrapperTmpl<S>::solve(void) //// STOPPED HERE
{
  assert(solver != nullptr && isvalid);
  DBG(cout << "[minisat-abbr] hitting the SAT solver (#cls = "
           << solver->nClauses() << ", #vars = " << solver->nVars()
           << ", #assum = " << assumps.size() << ")" << endl;);
  DBG(cout << "[minisat-abbr] assumptions: ";
      for (int i = 0; i < assumps.size(); ++i)
        cout << (sign(assumps[i]) ? "-" : "") << var(assumps[i]) << " ";
      cout << endl;);
  if (verbosity >= 10) { prt_std_cputime("c ", "Running SAT solver ..."); }
  if (max_confls == -1)
    solver->budgetOff();
  else
    solver->setConfBudget(max_confls);
  lbool res = (simp
      ? static_cast<SimpSolver*>(solver)->solveLimited(assumps, false, false) // no prepro by default
      : solver->solveLimited(assumps));
  SATRes status = (res == l_True) ? SAT_True : ((res == l_False) ? SAT_False  : SAT_NoRes);
  if (verbosity >= 10) { prt_std_cputime("c ", "Done running SAT solver ... "); }
  if (status == SAT_True && need_model) { handle_sat_outcome(); }
  else if (status == SAT_False && need_core) { handle_unsat_outcome(); }
  return status;
}

template<class S>
void MinisatAbbrLowLevelWrapperTmpl<S>::handle_sat_outcome(void)
{
  model.resize(nvars());
  for(int i = 1; i < nvars(); ++i) {
    model[i] = ((toInt(solver->modelValue((Var)i)) == 0) ? 1 : -1);
  }
  DBG(cout << "[minisat-abbr] model: ";
      for (unsigned i = 1; i < model.size(); ++i)
        if (model[i] != 0) { cout << model[i]*i << " "; }
      cout << endl;);
}

template<class S>
void MinisatAbbrLowLevelWrapperTmpl<S>::handle_unsat_outcome(void)
{
  assert(ucore.size() == 0);
  for(int i = 0; i < solver->conflict.size(); ++i)
    ucore.push_back(var(solver->conflict[i]));
  DBG(cout << "[minisat-abbr] core assumptions: ";
      for (ULINT v : ucore) cout << v << " "; cout << endl;);
}

// Preprocessing
template<class S>
SATRes MinisatAbbrLowLevelWrapperTmpl<S>::preprocess(bool turn_off)
{
  tool_warn("preprocessing in minisat-abbr has not been tested");
  bool res = true;
  solver->verbosity = (verbosity > 5) ? 5 : 0; // b/c preprocess can be called before the rest
  res = simp
      ? static_cast<SimpSolver*>(solver)->eliminate(turn_off)   // true is to turn-off elimination after
      : solver->simplify();
  return (!res) ? SAT_False : SAT_NoRes;
}

template<class S>
void MinisatAbbrLowLevelWrapperTmpl<S>::freeze_var(ULINT var)
{
  if (simp) { static_cast<SimpSolver*>(solver)->setFrozen(var, true); }
}

template<class S>
void MinisatAbbrLowLevelWrapperTmpl<S>::unfreeze_var(ULINT var)
{
  if (simp) { static_cast<SimpSolver*>(solver)->setFrozen(var, false); }
}

template<class S>
void MinisatAbbrLowLevelWrapperTmpl<S>::_add_clause(ULINT svar, Literator pbegin, Literator pend)
{
  assert(solver != nullptr);
  assert(clits.size() == 0);
  if (svar) {
    update_maxvid(svar);
    clits.push(mkLit(svar, true));
    if (simp) { static_cast<SimpSolver*>(solver)->setFrozen(svar, true); }
  }
  for_each(pbegin, pend, [&](LINT lit) {
    this->update_maxvid(llabs(lit)); // this-> is a work-around of a bug in gcc 4.7 
    this->clits.push(mkLit(std::llabs(lit), lit<0));
  });
  solver->addClause(clits);
  DBG(cout << "[minisat-abbr] added " << (svar ? "" : "final ") << "clause ";
      if (svar) { cout << "[svar = " << svar << "] "; }
      for(int i = 0; i < clits.size(); ++i)
        cout << (sign(clits[i]) ? "-" : "") << var(clits[i]) << " ";
      cout << "0" << endl;);
  clits.clear();
}

// this is to instantiate the templates -- allows to keep in the implementation
// in .cc file; if you need another type, add it here
template class MinisatAbbrLowLevelWrapperTmpl<Solver>;
//template class MinisatAbbrLowLevelWrapperTmpl<SimpSolver>;

/*----------------------------------------------------------------------------*/
