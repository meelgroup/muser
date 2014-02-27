//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        minisat22_ll_wrapper_incr.cc
 *
 * Description: 
 *
 * Author:      jpms
 *
 *                                     Copyright (c) 2012, Joao Marques-Silva
\*----------------------------------------------------------------------------*/
//jpms:ec

#include "globals.hh"
#include "minisat22_ll_wrapper_incr.hh"

using namespace Minisat;

// Constructor. Calls minisat22_init()
template<class S>
Minisat22LowLevelWrapperTmpl<S>::Minisat22LowLevelWrapperTmpl(IDManager& _imgr) :
  SATSolverLowLevelWrapper(_imgr), solver(NULL), clits(), assumps()
{
  solver = new S();
  simp = (dynamic_cast<Minisat::SimpSolver*>(solver) != NULL);
}

template<class S>
Minisat22LowLevelWrapperTmpl<S>::~Minisat22LowLevelWrapperTmpl() {
  isvalid = false;
  if (solver != NULL) {
    delete solver;
    solver = NULL;
  }
  clits.clear();
  assumps.clear();
}

template<class S>
void Minisat22LowLevelWrapperTmpl<S>::init_run()
{
  assert(!isvalid);
  model.clear(); ucore.clear(); isvalid = true;
  solver->verbosity = (verbosity > 20) ? 5 : 0;
  solver->rnd_pol = (phase == 0);
}

template<class S>
void Minisat22LowLevelWrapperTmpl<S>::reset_run()
{
  assert(isvalid);
  assumps.clear(); model.clear(); ucore.clear(); isvalid = false;
  solver->resetRun();
  solver->verbosity = 0;
}

template<class S>
void Minisat22LowLevelWrapperTmpl<S>::reset_solver()
{
  DBG(cout << "incr resetting all ...\n";);
  if (solver != NULL) {
    delete solver;
    solver = new S();
    solver->verbosity = (verbosity > 20) ? 5 : 0;
    solver->rnd_pol = (phase == 0);
  }
  assumps.clear(); model.clear(); ucore.clear(); isvalid = false;
}

template<class S>
SATRes Minisat22LowLevelWrapperTmpl<S>::solve()
{
  DBG(cout<<"Executing Minisat22LowLevelWrapperTmpl<S>::solve ...\n"; cout.flush(););
  assert(isvalid);
  DBG(cout<<"Vars in solver:    " << nvars() << endl;
      cout<<"Clauses in solver: " << ncls() << endl;);

  DBG(cout<<"ASSUMP SIZE: "<<assumps.size()<<endl;
      cout << "ASSUMPS: [";for(int i=0; i<assumps.size(); ++i) { cout << toInt(assumps[i]) << " "; } cout<<"]\n";);

  DBG(print_cnf("formula.cnf"););

  // 1. Invoke SAT solver
  if (verbosity >= 20) { prt_std_cputime("c ", "Running SAT solver ..."); }
  if (max_confls == -1)
    solver->budgetOff();
  else
    solver->setConfBudget(max_confls);
  lbool res = (simp
      ? static_cast<Minisat::SimpSolver*>(solver)->solveLimited(assumps, false, false) // no prepro by default
      : solver->solveLimited(assumps));
  SATRes status = (res == l_True) ? SAT_True : ((res == l_False) ? SAT_False  : SAT_NoRes);
  DBG(cout << "Status: " << status << endl;
      cout << "Vars in solver: " << solver->nVars() << endl;);
  if (verbosity >= 10) { prt_std_cputime("c ", "Done running SAT solver... "); }

  // 2. Analyze outcome and get relevant data
  if (status == SAT_False && need_core) {             // Unsatisfiable
    handle_unsat_outcome();
  }
  else if (status == SAT_True && need_model) {        // Satisfiable
    handle_sat_outcome();
  }
  return status;
}

template<class S>
void Minisat22LowLevelWrapperTmpl<S>::handle_sat_outcome()
{
  assert(model.size() == 0);
  if ((int) model.size() < solver->nVars()) {    // Obs: solver w/ one more var
    assert(solver->nVars() == solver->model.size());
    model.resize(solver->nVars(), 0);
  }
  for(int i=1; i < solver->nVars(); ++i) {
    assert(i < solver->model.size());
    model[i] = (toInt(solver->modelValue((Var)i)) == 0) ? 1 : -1;
    DBG(cout << "id " << i << "=" << model[i] << endl;);
  }
  DBG(cout<<"Model size: "<<model.size()<<endl;
      //cout<<"INT_MAX:  "<<INT_MAX<<endl;
      //cout<<"LONG_MAX: "<<LONG_MAX<<endl;
      //FILE* fp = fopen("formula2.cnf", "w");
      //for(int i=1; i<=solver->nVars(); ++i) {
      //if (model[i] > 0)      { fprintf(fp, "%d 0\n", i); }
      //else if (model[i] < 0) { fprintf(fp, "-%d 0\n", i); }
      //}
      //fclose(fp); 
      print_cnf("formula.cnf"); );
}

template<class S>
void Minisat22LowLevelWrapperTmpl<S>::handle_unsat_outcome()
{
  assert(ucore.size() == 0);
  compute_unsat_core();
}

// Compute unsat core given assumptions in final clause
template<class S>
void Minisat22LowLevelWrapperTmpl<S>::compute_unsat_core()
{
  for(int i=0; i<solver->conflict.size(); ++i) {
    ucore.push_back(var(solver->conflict[i]));
  }
}

// Preprocessing
template<class S>
SATRes Minisat22LowLevelWrapperTmpl<S>::preprocess(bool turn_off)
{
  bool res = true;
  solver->verbosity = (verbosity > 5) ? 5 : 0; // b/c preprocess can be called before the rest
  if (simp) {
    res = static_cast<Minisat::SimpSolver*>(solver)->eliminate(turn_off);   // true is to turn-off elimination after
  } else {
    res = static_cast<Minisat::Solver*>(solver)->simplify();
  }
  return (!res) ? SAT_False : SAT_NoRes;
}

template<class S>
void Minisat22LowLevelWrapperTmpl<S>::freeze_var(ULINT var)
{
  if (simp) {
    static_cast<Minisat::SimpSolver*>(solver)->setFrozen(var, true);
  }
}

template<class S>
void Minisat22LowLevelWrapperTmpl<S>::unfreeze_var(ULINT var)
{
  if (simp) {
    static_cast<Minisat::SimpSolver*>(solver)->setFrozen(var, false);
  }
}

/** Adds to cset the clauses that are actually inside the underlying
 * SAT solver -- useful for getting the preprocessed instances back.
 */
template<class S>
void Minisat22LowLevelWrapperTmpl<S>::get_solver_clauses(BasicClauseSet& cset)
{
  vec<const Clause*> cls;
  const vec<Lit>* p_trail = NULL; 
  solver->getClauses(cls, p_trail);
  assert(p_trail != NULL);
  vector<LINT> clits;
  for (int i = 0; i < cls.size(); i++) {
    const Clause& cl = *cls[i];
    clits.clear();
    clits.resize(cl.size());
    for (int j = 0; j < cl.size(); ++j)
      clits[j] = sign(cl[j]) ? -var(cl[j]) : var(cl[j]);
    cset.create_clause(clits);
  }
  for (int i = 0; i < p_trail->size(); i++) {
    Lit l = (*p_trail)[i];
    cset.create_unit_clause(sign(l) ? -var(l) : var(l));
  }
}


template<class S>
void Minisat22LowLevelWrapperTmpl<S>::print_cnf(const char* fname)
{
  //assert(assumps.size() == 0);
  FILE* fp = fopen(fname, "w");
  solver->toDimacs(fp, assumps);
  fclose(fp);
}


// this is to instantiate the templates -- allows to keep in the implementation
// in .cc file; if you need another type, add it here
template class Minisat22LowLevelWrapperTmpl<Minisat::Solver>;
template class Minisat22LowLevelWrapperTmpl<Minisat::SimpSolver>;

/*----------------------------------------------------------------------------*/
