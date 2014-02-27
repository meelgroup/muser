//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        minisat-hmuc_llni_wrapper.cc
 *
 * Description: implementation of MinisatHMUC non-incremental wrapper
 *
 * Author:      antonb
 *
 *                                              Copyright (c) 2013, Anton Belov
\*----------------------------------------------------------------------------*/
//jpms:ec

#include <algorithm>
#include "globals.hh"
#include "minisat-hmuc_llni_wrapper.hh"

using namespace MinisatHMUC;

//#define DBG(x) x

// Constructor
MinisatHMUCLowLevelNonIncrWrapper::MinisatHMUCLowLevelNonIncrWrapper(IDManager& _imgr) :
  SATSolverLowLevelNonIncrWrapper(_imgr)
{
  //tool_warn("minisat-hmuc(ni) wrapper has not been thoroughly tested");
}

MinisatHMUCLowLevelNonIncrWrapper::~MinisatHMUCLowLevelNonIncrWrapper(void)
{
  if (solver != nullptr) { delete solver; solver = nullptr; }
}

void MinisatHMUCLowLevelNonIncrWrapper::init_solver(void)
{
  assert(solver == nullptr && !inited);
  Clause::SetUid(1);
  solver = new SimpSolver();
  uid2cl.clear();
  inited = true;
  ready = false;
  ic_confl = false;
  solver->verbosity = (verbosity > 20) ? 5 : 0;
  solver->rnd_pol = (phase == 0);
}

void MinisatHMUCLowLevelNonIncrWrapper::init_run(void)
{
  assert(solver != nullptr && inited && !ready);
  model.clear(); ucore.clear(); ready = true;
}

void MinisatHMUCLowLevelNonIncrWrapper::reset_run(void)
{
  assert(solver != nullptr && inited);
  model.clear(); ucore.clear(); ready = false;
}

void MinisatHMUCLowLevelNonIncrWrapper::reset_solver(void)
{
  assert(solver != nullptr && inited && !ready);
  if (solver != nullptr) { delete solver; solver = nullptr; }
  uid2cl.clear(); inited = false;
}

SATRes MinisatHMUCLowLevelNonIncrWrapper::solve(void)
{
  assert(solver != nullptr && inited && ready);
  DBG(cout << "[minisat-hmuc(ni)] hitting the SAT solver (#cls = "
           << solver->nClauses() << ", #vars = " << solver->nVars() << ")" << endl;);
  if (verbosity >= 10) { prt_std_cputime("c ", "Running SAT solver ..."); }
  bool status = ic_confl ? false : solver->solve(false, false);
  if (verbosity >= 10) { prt_std_cputime("c ", "Done running SAT solver ... "); }
  if (status && need_model) { handle_sat_outcome(); }
  else if (!status && need_core) { handle_unsat_outcome(); }
  ready = false; // this means that you have to reset the solver.
  return status ? SAT_True : SAT_False;
}

void MinisatHMUCLowLevelNonIncrWrapper::handle_sat_outcome(void)
{
  model.resize(solver->nVars());
  for(int i = 1; i < solver->nVars(); ++i) {
    model[i] = ((toInt(solver->modelValue((Var)i)) == 0) ? 1 : -1);
    NDBG(cout << "id " << i << "=" << model[i] << endl;);
  }
}

void MinisatHMUCLowLevelNonIncrWrapper::handle_unsat_outcome(void)
{
  assert(ucore.size() == 0);
  vec<uint32_t> uids;
  solver->GetUnsatCore(uids);
  DBG(cout << "[minisat-hmuc(ni)] unsat core size " << uids.size() << endl;);
  for (int i = 0; i < uids.size(); ++i) {
    assert(uids[i] < uid2cl.size() && uid2cl[uids[i]] != nullptr
           && "clause uid should be mapped to a real clause");
    ucore.push_back(uid2cl[uids[i]]);
    DBG(cout << "[minisat-hmuc(ni)] core clause [uid= " << uids[i] << "] " << *uid2cl[uids[i]] << endl;);
  }
}

// Preprocessing
SATRes MinisatHMUCLowLevelNonIncrWrapper::preprocess(bool turn_off)
{
  tool_warn("preprocessing in minisat-hmuc(ni) has not been tested");
  bool res = true;
  if (ic_confl) { return SAT_False; }
  solver->verbosity = (verbosity > 5) ? 5 : 0; // b/c preprocess can be called before the rest
  res = solver->eliminate(turn_off);   // true is to turn-off elimination after
  if (!res) { ic_confl = true; }
  return (!res) ? SAT_False : SAT_NoRes;
}

int MinisatHMUCLowLevelNonIncrWrapper::_add_clause(BasicClause* cl)
{
  // +HACK: the solver seems to be having problems with computing UNSAT core in the
  // presence of tautological clauses; not good
  if (cl->is_tautology()) { return -1; }
  // -HACK
  int uid = _add_clause(cl->begin(), cl->end());
  if (uid >= 0) {
    uid2cl.resize(uid + 1, nullptr);
    uid2cl[uid] = cl;
  }
  return uid;
}

int MinisatHMUCLowLevelNonIncrWrapper::_add_clause(Literator pbegin, Literator pend)
{
  assert(solver != nullptr && inited && !ready
         && "solver must be between inited and ready states");
  assert(clits.size() == 0);
  if (ready) { tool_abort("minisat-hmuc(ni) wrapper does not support clause addition after solve()"); }
  if (ic_confl) { return -1; }
  for_each(pbegin, pend, [&](LINT lit) {
    ULINT var = std::llabs(lit);
    while(var >= (ULINT)solver->nVars()) { solver->newVar((bool)(phase == -1), true); }
    clits.push(MinisatHMUC::mkLit(var, lit<0)); });
  solver->addClause(clits, true);
  uint32_t uid = Clause::GetLastUid();
  if (!solver->okay()) {
    solver->CreateResolVertex(uid);
    solver->AddConflictingIc(uid);
    ic_confl = true;
  }
  DBG(cout << "[minisat-hmuc(ni)] added clause [uid = " << uid << "] ";
      for(int i = 0; i < clits.size(); ++i)
        cout << (sign(clits[i]) ? "-" : "") << var(clits[i]) << " ";
      cout << "0" << endl;
      if (ic_confl)
        cout << "[minisat-hmuc(ni)] top-level conflict while adding clauses; no more will be added." << endl;);
  clits.clear();
  return (int)uid;
}


/*----------------------------------------------------------------------------*/
