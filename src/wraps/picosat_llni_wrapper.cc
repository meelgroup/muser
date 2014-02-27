//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        picosat_llni_wrapper.cc
 *
 * Description: Low-level non-incremental wrapper for re-entrant Picosat (v953+)
 *
 * Author:      antonb
 *
 *                                              Copyright (c) 2013, Anton Belov
\*----------------------------------------------------------------------------*/
//jpms:ec

#include <algorithm>
#include "globals.hh"
#include "picosat_llni_wrapper.hh"

using namespace std;

//#define DBG(x) x

// Constructor.
PicosatLowLevelNonIncrWrapper::PicosatLowLevelNonIncrWrapper(IDManager& _imgr) :
  SATSolverLowLevelNonIncrWrapper(_imgr)
{
}

PicosatLowLevelNonIncrWrapper::~PicosatLowLevelNonIncrWrapper(void)
{
  if (solver != nullptr) { Picosat954TR::picosat_reset(solver); solver = nullptr; }
}

void PicosatLowLevelNonIncrWrapper::init_solver(void)
{
  assert(solver == nullptr && !inited);
  solver = Picosat954TR::picosat_init();
  Picosat954TR::picosat_enable_trace_generation(solver);
  id2cl.clear();
  inited = true;
  ready = false;
  Picosat954TR::picosat_set_verbosity(solver, max((int)verbosity - 10, 0));
  if (phase != 3)
    Picosat954TR::picosat_set_global_default_phase(solver, (phase == 2) ? 3 : phase);
}

void PicosatLowLevelNonIncrWrapper::init_run(void)
{
  assert(solver != nullptr && inited && !ready);
  model.clear(); ucore.clear(); ready = true;
}

void PicosatLowLevelNonIncrWrapper::reset_run(void)
{
  assert(solver != nullptr && inited);
  model.clear(); ucore.clear(); ready = false;
}

void PicosatLowLevelNonIncrWrapper::reset_solver(void)
{
  assert(solver != nullptr && inited && !ready);
  if (solver != nullptr) { Picosat954TR::picosat_reset(solver); solver = nullptr; }
  id2cl.clear(); inited = false;
}


SATRes PicosatLowLevelNonIncrWrapper::solve(void)
{
  assert(solver != nullptr && inited && ready);
  DBG(cout << "[picosat(ni)] hitting the SAT solver (#cls = " << ncls()
           << ", #vars = " << nvars() << ")" << endl;);
  if (phase != 3) // needs to be re-done here
    Picosat954TR::picosat_set_global_default_phase(solver, (phase == 2) ? 3 : phase);
  if (verbosity >= 10) { prt_std_cputime("c ", "Running SAT solver ..."); }
  int status = Picosat954TR::picosat_sat(solver, -1);    // No limit on decisions
  if (verbosity >= 10) { prt_std_cputime("c ", "Done running SAT solver ... "); }
  if (status != 10 && status != 20) { return SAT_Abort; }
  if (status == 10 && need_model) { handle_sat_outcome(); }
  else if (status == 20 && need_core) { handle_unsat_outcome(); }
  return (status == 10) ? SAT_True : SAT_False;
}

void PicosatLowLevelNonIncrWrapper::handle_sat_outcome(void)
{
  model.resize(nvars());
  for(int i = 1; i < (int)nvars(); ++i)
    model[i] = Picosat954TR::picosat_deref(solver, i);
  DBG(cout << "[picosat(ni)] model: ";
      for (unsigned i = 1; i < model.size(); ++i)
        if (model[i] != 0) { cout << model[i]*i << " "; }
      cout << endl;);
}

void PicosatLowLevelNonIncrWrapper::handle_unsat_outcome(void)
{
  assert(ucore.size() == 0);
  for (size_t cl_id = 0; cl_id < id2cl.size(); ++cl_id)
    if ((id2cl[cl_id] != nullptr) && Picosat954TR::picosat_coreclause(solver, cl_id))
        ucore.push_back(id2cl[cl_id]);
  DBG(cout << "[picosat(ni)] unsat core size " << ucore.size() << endl;
      for (BasicClause* cl : ucore)
        cout << "[picosat(ni)] core clause: " << *cl << endl;);
}

int PicosatLowLevelNonIncrWrapper::_add_clause(BasicClause* cl, function<bool(LINT lit)>* skip_lit)
{
  int cl_id = _add_clause(cl->begin(), cl->end(), skip_lit);
  id2cl.resize(cl_id + 1, nullptr);
  id2cl[cl_id] = cl;
  return cl_id;
}

int PicosatLowLevelNonIncrWrapper::_add_clause(Literator pbegin, Literator pend,
                                               function<bool(LINT lit)>* skip_lit)
{
  assert(solver != nullptr && inited && !ready
         && "solver must be between inited and ready states");
  if (ready) { tool_abort("picosat(ni) wrapper does not support clause addition after solve()"); }
  if (skip_lit != nullptr) {
    for_each(pbegin, pend, [&](LINT lit) { if (!(*skip_lit)(lit)) Picosat954TR::picosat_add(solver, lit); });
  } else {
    for_each(pbegin, pend, [&](LINT lit) { Picosat954TR::picosat_add(solver, lit); });
  }
  int cl_id = Picosat954TR::picosat_add(solver, 0);
  DBG(cout << "[picosat(ni)] added clause [id = " << cl_id << "] ";
      for_each(pbegin, pend, [&](LINT lit) { 
          if ((skip_lit == nullptr) or !(*skip_lit)(lit)) cout << lit << " "; });
      cout << "0" << endl;);
  return cl_id;
}

/*----------------------------------------------------------------------------*/
