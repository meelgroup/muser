//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        IPASIR_ll_wrapper.cc
 *
 * Description: Low-level incremental wrapper for re-entrant IPASIR
 *
 * Author:      nmanthey
 *
 *                                          Copyright (c) 2017, Norbert Manthey
\*----------------------------------------------------------------------------*/
//jpms:ec

#ifdef IPASIR_LIB

#include <algorithm>
#include "globals.hh"
#include "IPASIR_ll_wrapper.hh"

using namespace std;

//#define DBG(x) x

static const int abs_max(const unsigned int a, const int b)
{
  const int abs_b = (b >= 0) ? b : -b;
  return a > abs_b ? a : abs_b;
}

// Constructor.
IPASIRLowLevelWrapper::IPASIRLowLevelWrapper(IDManager& _imgr) :
  SATSolverLowLevelWrapper(_imgr)
{
  solver = ipasir_init();
}

IPASIRLowLevelWrapper::~IPASIRLowLevelWrapper(void)
{
  if (solver != nullptr) { ipasir_release(solver); solver = nullptr; max_var = 0;}
}

void IPASIRLowLevelWrapper::init_run(void)
{
  assert(!isvalid);
  model.clear(); ucore.clear(); isvalid = true;

#if 0
  ipasir_set_verbosity(solver, max((int)verbosity - 10, 0));
  // phases in picosat: 0 = false, 1 = true, 2 = Jeroslow-Wang (default), 3 = random
  if (phase != 3)
    ipasir_set_global_default_phase(solver, (phase == 2) ? 3 : phase);
#endif
}

void IPASIRLowLevelWrapper::reset_run(void)
{
  assert(isvalid);
  assumps.clear(); model.clear(); ucore.clear(); isvalid = false;
}

void IPASIRLowLevelWrapper::reset_solver(void)
{
  DBG(cout << "[IPASIR] reset solver" << endl;);
  if (solver != nullptr) {
    ipasir_release(solver);
    max_var = 0;
    solver = ipasir_init();
  }
  assumps.clear(); model.clear(); ucore.clear(); isvalid = false;
}

SATRes IPASIRLowLevelWrapper::solve(void)
{
  assert(solver != nullptr && isvalid);
  DBG(cout << "[IPASIR] hitting the SAT solver (#cls = " << ncls()
           << ", #vars = " << nvars() << ", #assum = " << assumps.size()
           << ")" << endl;);
  DBG(cout << "[pisosat] assumptions:";
      for (int a : assumps) { cout << " " << a; } cout << endl;);
  for (int a : assumps) { ipasir_assume(solver, a); max_var = abs_max(max_var, a); }
  if (verbosity >= 10) { prt_std_cputime("c ", "Running SAT solver ..."); }
  int status = ipasir_solve(solver);    // No limit on decisions
  if (verbosity >= 10) { prt_std_cputime("c ", "Done running SAT solver ... "); }
  if (status != 10 && status != 20) { return SAT_Abort; }
  if (status == 10 && need_model) { handle_sat_outcome(); }
  else if (status == 20 && need_core) { handle_unsat_outcome(); }
  return (status == 10) ? SAT_True : SAT_False;
}

void IPASIRLowLevelWrapper::handle_sat_outcome(void)
{
  model.resize(nvars()+1);
  for(int i = 1; i < (int)nvars() + 1; ++i)
  {
    int v = ipasir_val(solver, i);
    model[i] = v > 0 ? 1 : -1;
  }
  DBG(cout << "[IPASIR] model: ";
      for (int i = 1; i < model.size(); ++i)
        if (model[i] != 0) { cout << "(" << i << ":)" << model[i]*i << " "; }
      cout << endl;);
}

void IPASIRLowLevelWrapper::handle_unsat_outcome(void)
{
  assert(ucore.size() == 0);
  for (int a : assumps)
    if (ipasir_failed(solver, a)) { ucore.push_back(abs(a)); }
  DBG(cout << "[IPASIR] core assumptions: ";
      for (LINT v : ucore) cout << v << " "; cout << endl;);
}

void IPASIRLowLevelWrapper::_add_clause(ULINT svar, Literator pbegin, Literator pend)
{
  if (svar) { ipasir_add(solver, -svar); max_var = abs_max(max_var, svar); }
  for_each(pbegin, pend, [&](int lit) { ipasir_add(solver, lit); max_var = abs_max(max_var, lit); });
  ipasir_add(solver, 0);
  ++ added_clauses;
  DBG(cout << "[IPASIR] added " << (svar ? "" : "final ") << "clause ";
      if (svar) { cout << "[svar = " << svar << "] "; }
      for_each(pbegin, pend, [&](int lit) { cout << lit << " "; });
      cout << "0" << endl;);
}

#endif

/*----------------------------------------------------------------------------*/
