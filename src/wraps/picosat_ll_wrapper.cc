//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        picosat_ll_wrapper.cc
 *
 * Description: Low-level incremental wrapper for re-entrant Picosat (v953+)
 *
 * Author:      antonb
 *
 *                                              Copyright (c) 2013, Anton Belov
\*----------------------------------------------------------------------------*/
//jpms:ec

#include <algorithm>
#include "globals.hh"
#include "picosat_ll_wrapper.hh"

using namespace std;

//#define DBG(x) x

// Constructor.
PicosatLowLevelWrapper::PicosatLowLevelWrapper(IDManager& _imgr) :
  SATSolverLowLevelWrapper(_imgr)
{
  solver = Picosat954::picosat_init();
}

PicosatLowLevelWrapper::~PicosatLowLevelWrapper(void)
{
  if (solver != nullptr) { Picosat954::picosat_reset(solver); solver = nullptr; }
}

void PicosatLowLevelWrapper::init_run(void)
{
  assert(!isvalid);
  model.clear(); ucore.clear(); isvalid = true;
  Picosat954::picosat_set_verbosity(solver, max((int)verbosity - 10, 0));
  // phases in picosat: 0 = false, 1 = true, 2 = Jeroslow-Wang (default), 3 = random
  if (phase != 3)
    Picosat954::picosat_set_global_default_phase(solver, (phase == 2) ? 3 : phase);
}

void PicosatLowLevelWrapper::reset_run(void)
{
  assert(isvalid);
  assumps.clear(); model.clear(); ucore.clear(); isvalid = false;
}

void PicosatLowLevelWrapper::reset_solver(void)
{
  DBG(cout << "[picosat] reset solver" << endl;);
  if (solver != nullptr) {
    Picosat954::picosat_reset(solver);
    solver = Picosat954::picosat_init();
  }
  assumps.clear(); model.clear(); ucore.clear(); isvalid = false;
}

SATRes PicosatLowLevelWrapper::solve(void)
{
  assert(solver != nullptr && isvalid);
  DBG(cout << "[picosat] hitting the SAT solver (#cls = " << ncls()
           << ", #vars = " << nvars() << ", #assum = " << assumps.size()
           << ")" << endl;);
  DBG(cout << "[pisosat] assumptions:";
      for (int a : assumps) { cout << " " << a; } cout << endl;);
  for (int a : assumps) { Picosat954::picosat_assume(solver, a); }
  if (verbosity >= 10) { prt_std_cputime("c ", "Running SAT solver ..."); }
  int status = Picosat954::picosat_sat(solver, -1);    // No limit on decisions
  if (verbosity >= 10) { prt_std_cputime("c ", "Done running SAT solver ... "); }
  if (status != 10 && status != 20) { return SAT_Abort; }
  if (status == 10 && need_model) { handle_sat_outcome(); }
  else if (status == 20 && need_core) { handle_unsat_outcome(); }
  return (status == 10) ? SAT_True : SAT_False;
}

void PicosatLowLevelWrapper::handle_sat_outcome(void)
{
  model.resize(nvars());
  for(int i = 1; i < (int)nvars(); ++i)
    model[i] = Picosat954::picosat_deref(solver, i);
  DBG(cout << "[picosat] model: ";
      for (unsigned i = 1; i < model.size(); ++i)
        if (model[i] != 0) { cout << model[i]*i << " "; }
      cout << endl;);
}

void PicosatLowLevelWrapper::handle_unsat_outcome(void)
{
  assert(ucore.size() == 0);
  for (int a : assumps)
    if (Picosat954::picosat_failed_assumption(solver, a)) { ucore.push_back(abs(a)); }
  DBG(cout << "[picosat] core assumptions: ";
      for (LINT v : ucore) cout << v << " "; cout << endl;);
}

void PicosatLowLevelWrapper::_add_clause(ULINT svar, Literator pbegin, Literator pend)
{
  if (svar) { Picosat954::picosat_add(solver, -svar); }
  for_each(pbegin, pend, [&](int lit) { Picosat954::picosat_add(solver, lit); });
  Picosat954::picosat_add(solver, 0);
  DBG(cout << "[picosat] added " << (svar ? "" : "final ") << "clause ";
      if (svar) { cout << "[svar = " << svar << "] "; }
      for_each(pbegin, pend, [&](int lit) { cout << lit << " "; });
      cout << "0" << endl;);
}

/*----------------------------------------------------------------------------*/
