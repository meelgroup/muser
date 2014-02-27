//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        minisat-hmuc_ll_wrapper.cc
 *
 * Description: implementation of low-level incremental wrapper for Minisat-HMUC
 *
 * Author:      antonb
 *
 *                                              Copyright (c) 2013, Anton Belov
\*----------------------------------------------------------------------------*/
//jpms:ec

#include "globals.hh"
#include "minisat-hmuc_ll_wrapper.hh"

using namespace MinisatHMUC;

//#define DBG(x) x

// Constructor.
MinisatHMUCLowLevelWrapper::MinisatHMUCLowLevelWrapper(IDManager& _imgr) :
  SATSolverLowLevelWrapper(_imgr)
{
  tool_abort("minisat-hmuc incremental wrapper is broken (assumptions are not supported in the solver)");
  solver = new SimpSolver();
}

MinisatHMUCLowLevelWrapper::~MinisatHMUCLowLevelWrapper(void)
{
  if (solver != NULL) { delete solver; solver = nullptr; }
}

void MinisatHMUCLowLevelWrapper::init_run(void)
{
  assert(!isvalid);
  model.clear(); ucore.clear(); isvalid = true;
  solver->verbosity = (verbosity > 20) ? 5 : 0;
  solver->rnd_pol = (phase == 0);
}

void MinisatHMUCLowLevelWrapper::reset_run(void)
{
  assert(isvalid);
  assumps.clear(); model.clear(); ucore.clear(); isvalid = false;
  solver->verbosity = 0;
}

void MinisatHMUCLowLevelWrapper::reset_solver(void)
{
  DBG(cout << "[minisat-hmuc] reset solver" << endl;);
  if (solver != nullptr) {
    delete solver;
    solver = new SimpSolver();
    solver->verbosity = (verbosity > 5) ? 5 : 0;
    solver->rnd_pol = (phase == 0);
  }
  assumps.clear(); model.clear(); ucore.clear(); isvalid = false;
}

SATRes MinisatHMUCLowLevelWrapper::solve(void) //// STOPPED HERE
{
  assert(solver != nullptr && isvalid);
  DBG(cout << "[minisat-hmuc] hitting the SAT solver (#cls = "
           << solver->nClauses() << ", #vars = " << solver->nVars()
           << ", #assum = " << assumps.size() << ")" << endl;);
  DBG(cout << "[minisat-hmuc] assumptions: ";
      for (int i = 0; i < assumps.size(); ++i)
        cout << (sign(assumps[i]) ? "-" : "") << var(assumps[i]) << " ";
      cout << endl;);
  if (verbosity >= 10) { prt_std_cputime("c ", "Running SAT solver ..."); }
  if (max_confls == -1)
    solver->budgetOff();
  else
    solver->setConfBudget(max_confls);
  lbool res = solver->solveLimited(assumps, false, false); // no prepro by default
  SATRes status = (res == l_True) ? SAT_True : ((res == l_False) ? SAT_False  : SAT_NoRes);
  if (verbosity >= 10) { prt_std_cputime("c ", "Done running SAT solver ... "); }
  if (status == SAT_True && need_model) { handle_sat_outcome(); }
  else if (status == SAT_False && need_core) { handle_unsat_outcome(); }
  return status;
}

void MinisatHMUCLowLevelWrapper::handle_sat_outcome(void)
{
  model.resize(solver->nVars());
  for(int i = 1; i < solver->nVars(); ++i) {
    model[i] = ((toInt(solver->modelValue((Var)i)) == 0) ? 1 : -1);
  }
  DBG(cout << "[minisat-hmuc] model: ";
      for (unsigned i = 1; i < model.size(); ++i)
        if (model[i] != 0) { cout << model[i]*i << " "; }
      cout << endl;);
}

void MinisatHMUCLowLevelWrapper::handle_unsat_outcome(void)
{
  assert(ucore.size() == 0);
  for(int i = 0; i < solver->conflict.size(); ++i)
    ucore.push_back(var(solver->conflict[i]));
  DBG(cout << "[minisat-hmuc] core assumptions: ";
      for (ULINT v : ucore) cout << v << " "; cout << endl;);
}

// Preprocessing
SATRes MinisatHMUCLowLevelWrapper::preprocess(bool turn_off)
{
  tool_warn("preprocessing in minisat-hmuc has not been tested");
  bool res = true;
  solver->verbosity = (verbosity > 5) ? 5 : 0; // b/c preprocess can be called before the rest
  res =solver->eliminate(turn_off);   // true is to turn-off elimination after
  return (!res) ? SAT_False : SAT_NoRes;
}

void MinisatHMUCLowLevelWrapper::_add_clause(ULINT svar, Literator pbegin, Literator pend)
{
  assert(solver != nullptr);
  assert(clits.size() == 0);
  if (svar) {
    update_maxvid(svar);
    clits.push(MinisatHMUC::mkLit(svar, true));
    solver->setFrozen(svar, true);
  }
  for_each(pbegin, pend, [&](LINT lit) {
    update_maxvid(std::llabs(lit));
    clits.push(MinisatHMUC::mkLit(std::llabs(lit), lit<0));
  });
  solver->addClause(clits, false); // untraceable always
  DBG(cout << "[minisat-hmuc] added " << (svar ? "" : "final ") << "clause ";
      if (svar) { cout << "[svar = " << svar << "] "; }
      for(int i = 0; i < clits.size(); ++i)
        cout << (sign(clits[i]) ? "-" : "") << var(clits[i]) << " ";
      cout << "0" << endl;);
  clits.clear();
}

/*----------------------------------------------------------------------------*/
