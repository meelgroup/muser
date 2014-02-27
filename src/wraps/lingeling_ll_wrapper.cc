//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        lingeling_ll_wrapper.cc
 *
 * Description: Low-level incremental wrapper for Lingeling SAT solver
 *
 * Author:      antonb
 *
 *                                              Copyright (c) 2013, Anton Belov
\*----------------------------------------------------------------------------*/
//jpms:ec

#include <algorithm>
#include "globals.hh"
#include "lingeling_ll_wrapper.hh"

using namespace LingelingALA;

//#define DBG(x) x

// Constructor.
LingelingLowLevelWrapper::LingelingLowLevelWrapper(IDManager& _imgr) :
  SATSolverLowLevelWrapper(_imgr)
{
  solver = lglinit();
  lglsetopt(solver, "simplify", 0);
}

LingelingLowLevelWrapper::~LingelingLowLevelWrapper(void)
{
  if (solver != nullptr) { lglrelease(solver); solver = nullptr; }
}

void LingelingLowLevelWrapper::init_run(void)
{
  assert(!isvalid);
  model.clear(); ucore.clear(); isvalid = true;
  lglsetopt(solver, "verbose", max((int)verbosity - 20, 0));
  // phases in lingeling: -1,+1; 0 is random (and is default)
  if (phase != 3)
    lglsetopt(solver, "phase", (phase == 0) ? -1 : ((phase == 1) ? 1 : 0));
}

void LingelingLowLevelWrapper::reset_run(void)
{
  assert(isvalid);
  assumps.clear(); model.clear(); ucore.clear(); isvalid = false;
}

void LingelingLowLevelWrapper::reset_solver(void)
{
  DBG(cout << "[lingeling] reset solver" << endl;);
  if (solver != nullptr) {
    lglrelease(solver);
    solver = lglinit();
    lglsetopt(solver, "simplify", 0);
  }
  assumps.clear(); model.clear(); ucore.clear(); isvalid = false;
}

SATRes LingelingLowLevelWrapper::solve(void)
{
  assert(solver != nullptr && isvalid);
  DBG(cout << "[lingeling] hitting the SAT solver (#cls = " << ncls()
           << ", #vars = " << nvars() << ", #assum = " << assumps.size()
           << ")" << endl;);
  DBG(cout << "[lingeling] assumptions:";
      for (int a : assumps) { cout << " " << a; } cout << endl;);
  for (int a : assumps) { lglassume(solver, a); }
  if (verbosity >= 10) { prt_std_cputime("c ", "Running SAT solver ..."); }
  int status = lglsat(solver);
  if (verbosity >= 10) {
    prt_std_cputime("c ", "Done running SAT solver ... ");
    if (verbosity >= 20) { lglstats(solver); }
  }
  if (status != 10 && status != 20) { return SAT_Abort; }
  if (status == 10 && need_model) { handle_sat_outcome(); }
  else if (status == 20 && need_core) { handle_unsat_outcome(); }
  return (status == 10) ? SAT_True : SAT_False;
}

void LingelingLowLevelWrapper::handle_sat_outcome(void)
{
  model.resize(nvars());
  for(int i = 1; i < (int)nvars(); ++i)
    model[i] = lglderef(solver, i);
  DBG(cout << "[lingeling] model: ";
      for (unsigned i = 1; i < (int)model.size(); ++i)
        if (model[i] != 0) { cout << model[i]*i << " "; }
      cout << endl;);
}

void LingelingLowLevelWrapper::handle_unsat_outcome(void)
{
  assert(ucore.size() == 0);
  for (int a : assumps)
    if (lglfailed(solver, a)) { ucore.push_back(abs(a)); }
  DBG(cout << "[lingeling] core assumptions: ";
      for (LINT v : ucore) cout << v << " "; cout << endl;);
}

// Preprocessing (optional)

SATRes LingelingLowLevelWrapper::preprocess(bool turn_off)
{
  SATRes res = SAT_NoRes;
  if (pre_on) {
    int r = lglsimp(solver, 1);
    res = (r == 10) ? SAT_True : (r == 20) ? SAT_False : res;
    if (turn_off) {
      lglsetopt(solver, "plain", 1);
      pre_on = false;
    }
  }
  return res;
}

// Warning: static is a potential problem in MT environment
extern "C" void lglctrav_callback(void * aux, int lit)
{
  static IntVector lits; // here !
  if (lit)
    lits.push_back(lit);
  else {
    BasicClauseSet& cset = *reinterpret_cast<BasicClauseSet*>(aux);
    cset.attach_clause(cset.create_clause(lits));
    lits.clear();
  }
}

// Warning: static is a potential problem in MT environment
extern "C" void lglutrav_callback(void * aux, int lit)
{
  BasicClauseSet& cset = *reinterpret_cast<BasicClauseSet*>(aux);
  cset.attach_clause(cset.create_unit_clause(lit));
}

void LingelingLowLevelWrapper::get_solver_clauses(BasicClauseSet& cset)
{
  lglctrav(solver, &cset, lglctrav_callback);
  lglutrav(solver, &cset, lglutrav_callback);
}

void LingelingLowLevelWrapper::_add_clause(ULINT svar, Literator pbegin, Literator pend)
{
  DBG(cout << "[lingeling] added " << (svar ? "" : "final ") << "clause ";
      if (svar) { cout << "[svar = " << svar << "] "; }
      for_each(pbegin, pend, [&](int lit) { cout << lit << " "; });
      cout << "0" << endl;);
  if (svar) { lgladd(solver, -svar); lglfreeze(solver, svar); }
  for_each(pbegin, pend, [&](int lit) { lgladd(solver, lit); });
  lgladd(solver, 0);
  ++num_cls;
}

/*----------------------------------------------------------------------------*/
