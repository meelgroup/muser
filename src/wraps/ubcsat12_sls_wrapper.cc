//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        ubcsat12_sls_wrapper.cc
 *
 * Description: SAT solver wrapper for ubcsat 1.2
 *
 * Author:      antonb
 * 
 *                                     Copyright (c) 2012, Anton Belov
\*----------------------------------------------------------------------------*/
//jpms:ec

#include "basic_clause.hh"
#include "basic_clset.hh"
#include "globals.hh"
#include "solver_utils.hh"
#include "ubcsat12_sls_wrapper.hh"
#include "ubcsat.h"

//#define DBG(x) x
//#define CHK(x) x

using namespace SolverUtils;
using namespace ubcsat;

namespace {     // local declarations

  // retrieves the current assignment quality 
  inline UBIGINT _ass_quality(bool weighted) {
    return (weighted) ? iSumFalseWeight : iNumFalse;
  }

}

// the instance
Ubcsat12SLSWrapper* Ubcsat12SLSWrapper::_pinstance = 0; 


/** Initializes all internal data structs; call once per life-time */
void Ubcsat12SLSWrapper::init_all(void)
{
  DBG(cout << "+Ubcsat12SLSWrapper::init_all()" << endl;);
  ensure_state(CREATED);
  StartAbsoluteClock();
  InitSeed();
  SetupUBCSAT();
  AddAlgorithms();
  AddParameters();
  AddReports();
  AddDataTriggers();    // Note that this one doesn't add ReadCNF anymore
  AddReportTriggers();  // not using report
  // if there are any unsupported  supported parameters, try to add them
  // here; however there is a chance that they might not work. e.g.
  //set_ubcsat_param("-drestart", "1000"); // dynamic restart (restart if no improvement in N steps)
  _prepare_params();
  if (verbosity >= 10) { _print_params(cout); }
  ParseAllParameters(_argc,const_cast<char**>(_argv)); // important !
  // it might be that setting these parameters is not necessary here
  if (weighted)  { iTargetWeight = trgt_quality; } else { iTarget = trgt_quality; }
  iNumRuns = tries;
  iCutoff = cutoff;     
  iWp = FloatToProb(wp);
  iMaxBreakValue = max_break_value;
  ActivateAlgorithmTriggers();
  ActivateTriggers("CheckTimeout,NoImprove"); // for -gtimeout, -noimprove
   //ActivateReportTriggers();
  RandomSeed(iSeed);
  RunProcedures(PostParameters);
  iNumVars = iNumClauses = iNumLits = 0;
  iMaxClauseLen = 0;
  iTotalClauseWeight = 0;
  num_runs = num_solved = num_improved = 0;
  set_state(INITIALIZED);
  DBG(cout << "-Ubcsat12SLSWrapper::init_all()" << endl;);
}

/** Inverse of init_all(), call before descruction */
void Ubcsat12SLSWrapper::reset_all(void)
{
  DBG(cout << "+Ubcsat12SLSWrapper::reset_all()" << endl;);
  ensure_state(INITIALIZED);
  _clear_storage();
  _cleanup_params();
  set_state(CREATED);
  DBG(cout << "-Ubcsat12SLSWrapper::reset_all()" << endl;);
}

/** Prepares for SAT run; call before each solve() */
void Ubcsat12SLSWrapper::init_run(void)
{
  DBG(cout << "+Ubcsat12SLSWrapper::init_run()" << endl;);
  ensure_state(INITIALIZED);
  if (weighted)  { iTargetWeight = trgt_quality; } else { iTarget = trgt_quality; }
  iNumRuns = tries;
  iCutoff = cutoff;
  iWp = FloatToProb(wp);
  fGlobalTimeOut = timeout;
  iNoImprove = noimprove;
  init_quality = final_quality = 0;
  iMaxBreakValue = max_break_value;
  set_state(PREPARED);
  DBG(cout << "-Ubcsat12SLSWrapper::init_run()" << endl;);
}


/** Inverse of init_run(), call after solve() */
void Ubcsat12SLSWrapper::reset_run(void)
{
  DBG(cout << "+Ubcsat12SLSWrapper::reset_run()" << endl;);
  ensure_state((State)(PREPARED | SOLVED));
  set_state(INITIALIZED);
  DBG(cout << "-Ubcsat12SLSWrapper::reset_run()" << endl;);
}

/** Returns the maximum variable id in the solver */
ULINT Ubcsat12SLSWrapper::max_var(void) const { return iNumVars; }

/** Notifies the wrapper of a change in the weight of the specified clause; the
 * clause is identified by its ID (get_id()); returns true if weight is updated
 * successfully (this requires that the solver is weighted and the clause with
 * the ID is found).
 */
bool Ubcsat12SLSWrapper::update_clause_weight(const BasicClause* cl)
{
  ensure_state(INITIALIZED);
  if (!weighted) { return false; }
  ClidMap::iterator p = _cl_map.find(cl->get_id());
  if (p == _cl_map.end()) { return false; }
  unsigned ubid = p->second;
  assert(ubid >= 0 && ubid < iNumClauses && "UBCSAT clause id should be in range");
  iTotalClauseWeight += cl->get_weight() - _cl_weights[ubid];
  _cl_weights[ubid] = cl->get_weight();
  return true;
}


// private stuff ...

/** Find an assignment or an approximation. If init_ass is not 0, its used to 
 * as the initial assignment, otherwise the intial assignment is random.
 */
SATRes Ubcsat12SLSWrapper::_solve(const IntVector* init_assign)
{
  DBG(cout << "+Ubcsat12SLSWrapper::_solve()" << endl;);
  ensure_state(PREPARED);
  SATRes result = SAT_Unknown;

  if (verbosity >= 10) { _print_params(cout); }

  // note that at this point I already "have" the instance
  RunProcedures(PostRead);
  RunProcedures(CreateData);            // for now, this will do re-allocations
  RunProcedures(CreateStateInfo);       // for now, this will do re-allocations
                                        // may consider to not do the allocations done by CreateDefaultStateInfo()

  // main loop (exracted from ubcsat.c) TODO: cleanup
  iRun = 0;
  iNumSolutionsFound = 0;
  bTerminateAllRuns = 0;
  RunProcedures(PreStart);
  // initial assignment (used for each run)
  if (init_assign)
    for (unsigned j = 1 ; j <= iNumVars; j++)
      aVarInit[j] = ((*init_assign)[j] > 0) ? 1 : 0; 
  else     
    for (unsigned j = 1 ; j <= iNumVars; j++)
      aVarInit[j] = 2;
  StartTotalClock();
  while ((iRun < iNumRuns) && (! bTerminateAllRuns)) {
    iRun++;
    iStep = 0;
    bSolutionFound = 0;
    bTerminateRun = 0;
    bRestart = 1;
    RunProcedures(PreRun);
    StartRunClock();
    while ((iStep < iCutoff) && (! bSolutionFound) && (! bTerminateRun)) {
      iStep++;
      iFlipCandidate = 0;
      RunProcedures(PreStep);
      RunProcedures(CheckRestart);
      if (bRestart) {
        RunProcedures(PreInit);
        RunProcedures(InitData);
        RunProcedures(InitStateInfo);
        init_quality = _ass_quality(weighted);
        if (verbosity >= 10)
          cout << "c SLS: " << ((iStep == 1) ? "initial" : "restart")
               << " solution quality=" << init_quality << endl;
        RunProcedures(PostInit);
        bRestart = 0;
      } else {
        RunProcedures(ChooseCandidate);
        RunProcedures(PreFlip);
        RunProcedures(FlipCandidate);
        RunProcedures(UpdateStateInfo);
        RunProcedures(PostFlip);
      }
      RunProcedures(PostStep);
      RunProcedures(CheckTerminate);
    }
    StopRunClock();
    RunProcedures(PostRun);
    if (bSolutionFound) { // terminate right away
      iNumSolutionsFound++;
      bTerminateAllRuns = 1;
      result = SAT_True;
    } else {
      result = SAT_Unknown;
    }
  }
  StopTotalClock();
  final_quality = _ass_quality(weighted);
  if (bSolutionFound || final_quality < init_quality) {
    for (unsigned j = 1 ; j <= iNumVars; j++)
      assignment[j] = aVarValue[j] ? 1 : -1;
  }
  num_runs++;
  if (bSolutionFound) { num_solved++; }
  if (final_quality < init_quality) { num_improved++; }
  if (verbosity >= 10) {
    cout << "c SLS: solution " << ((bSolutionFound) ? "" : "NOT") << " found in " 
         << iRun << " runs, " << iStep << " steps"
         << ((bSolutionFound) ? "" 
             : ((final_quality < init_quality) ? ", but quality improved" 
                : ", and quality NOT improved"))
         << ", quality=" << final_quality << endl;
  }
  CHK(if (bSolutionFound) { _check_invariants(trgt_quality); }
      else if (final_quality < init_quality) { _check_invariants(final_quality); });
  //_dump_clauses(cout, 0);
  FreeRAM();            // this will release all extra structures created by algorithms
  set_state(SOLVED);
  DBG(cout << "-Ubcsat12SLSWrapper::_solve() - returning 0x" << hex << result << dec << endl;);
  return result;
}

/** Prepares for parameters string */
void Ubcsat12SLSWrapper::_prepare_params(void)
{
  if (_argv != 0)
    delete[] _argv;
  _argv = new const char*[7 + 2*_params.size()]; // update if changing extra params !
  _argc = 0;
  _argv[_argc++] = "ubcsat";
  _argv[_argc++] = "-alg";
  switch (algo) {
  case WALKSAT_SKC:
    _argv[_argc++] = "walksat"; break;
  case ADAPTNOVELTY_PLUS:
    _argv[_argc++] = "adaptnovelty+"; break;
  case CAPTAIN_JACK:
    _argv[_argc++] = "jack"; break;
  default:      
    throw logic_error("invalid setting of algorithm in ubcsat12 wrapper");
  }
  _argv[_argc++] = "-inst";
  _argv[_argc++] = "dummy.cnf";
  _argv[_argc++] = "-q";
  if (weighted) { _argv[_argc++] = "-w"; }
  // extras ...
  for (ParamMap::iterator pm = _params.begin(); pm != _params.end(); ++pm) {
    _argv[_argc++] = pm->first.c_str();
    if (pm->second.size())
      _argv[_argc++] = pm->second.c_str();
  }
}

/** Prints out current parameters settings */
void Ubcsat12SLSWrapper::_print_params(ostream& out)
{
  out << "c SLS " << ((state == CREATED) ? "startup" : "current") << " parameters:" 
      << " tries=" << tries << ", cutoff=" << cutoff 
      << ", target_quality=" << trgt_quality 
      << ", timeout=" << timeout
      << ", noimprove=" << noimprove
      << ", algo=";
  switch(algo) {
  case WALKSAT_SKC:
    cout << "walksat_skc(wp=" << wp << ")"; break;
  case ADAPTNOVELTY_PLUS:
    cout << "adaptnovelty+(wp=" << wp << ")"; break;
  case CAPTAIN_JACK:
    cout << "jack" ; break;
  }
  if (!_params.empty()) {
    out << " ubcsat extras: ";
    for (ParamMap::iterator pm = _params.begin(); pm != _params.end(); ++pm) {
      out << pm->first << " ";
      if (pm->second.size()) { out << pm->second << " "; }
    }
  }
  out << endl;
}

/** Prepares for parameters string */
void Ubcsat12SLSWrapper::_cleanup_params(void)
{
  if (_argv != 0) {
    delete[] _argv;
    _argv = 0;
  }
  _argc = 0;
}

/** Adds a new clause to the storage with the specified weight and bolt clause
 * ID (if known); updates all the relevant ubcsat global data
 */
void Ubcsat12SLSWrapper::_add_clause(CLiterator begin, CLiterator end,
                                     XLINT weight, ULINT id)
{
  unsigned cl_len = end - begin;
  _cl_lengths.push_back((UINT32)cl_len);
  _cl_lits.push_back(_store_literals(begin, end));
  assert(weight > 0 && "clause weights must be positive");
  _cl_weights.push_back((UBIGINT)weight);
  _cl_ids.push_back(id);
  if (id) { _cl_map[id] = _cl_ids.size() - 1; }
  // update globals
  for ( ; begin != end; ++begin) {
    UINT32 var = abs(*begin);
    if (var >= iNumVars) { iNumVars = var + 1; }
  }
  assignment.resize(iNumVars+1, 0);
  iNumClauses++;
  iNumLits += cl_len;
  iTotalClauseWeight += weight;
  if (iMaxClauseLen < cl_len) { iMaxClauseLen = cl_len; }
  aClauseLen = &_cl_lengths[0]; // this needs to be done b/c vector may re-allocate
  pClauseLits = &_cl_lits[0];
  aClauseWeight = &_cl_weights[0];
  iVARSTATELen = (iNumVars >> 3) + 1;   // as per ubcsat
  if ((iNumVars & 0x07)==0) { iVARSTATELen--; }
  DBG(cout << "  added clause "; copy(end-cl_len, end, ostream_iterator<LINT>(cout, " "));
      cout << "0 (w=" << weight << ")" << endl;);
}

/** Reserves space for the specified number of clauses (just an optimization) */
void Ubcsat12SLSWrapper::_reserve_space(unsigned num_clauses)
{
  _cl_lengths.reserve(num_clauses << 1);
  _cl_lits.reserve(num_clauses << 1);
  _cl_weights.reserve(num_clauses << 1);
}

/** Literal storage management routine -- important
 */
LITTYPE* Ubcsat12SLSWrapper::_store_literals(CLiterator begin, CLiterator end)
{
  const unsigned _chunk_size = 4096;    // chunk size
  unsigned cl_len = end - begin;
  if (_lit_storage.empty() || (cl_len > (_chunk_size - _ls_end))) { // new chunk
    _lit_storage.push_back(new LITTYPE[_chunk_size]);
    _ls_end = 0;
  }
  LITTYPE* curr_chunk = _lit_storage.back();
  for( ; begin != end; ++begin, ++_ls_end) 
    curr_chunk[_ls_end] = (*begin > 0) ? GetPosLit(abs(*begin)) : GetNegLit(abs(*begin)); 
  return &curr_chunk[_ls_end - cl_len];
}

/** Releases all manually allocated storage
 */
void Ubcsat12SLSWrapper::_clear_storage(void)
{
  for (vector<LITTYPE*>::iterator pc = _lit_storage.begin(); 
       pc != _lit_storage.end(); ++pc)
    delete[] *pc;
  _lit_storage.clear();
}

/** Invariant checking routine: make sure that the given assignment quality is 
 * indeed correct (i.e. the current quality is <= test_quality)
 */
void Ubcsat12SLSWrapper::_check_invariants(XLINT test_quality)
{
  cout << "WARNING: expensive test in Ubcsat12SLSWrapper; result: " << flush;
  XLINT quality = 0;
  for (unsigned i = 0; i < _cl_lengths.size(); i++) {
    UINT32 cl_len = _cl_lengths[i];
    LITTYPE* cl_lits = _cl_lits[i];
    UBIGINT cl_weight = _cl_weights[i];
    unsigned num_true = 0;
    for (unsigned j = 0; j < cl_len; j++) {
      LITTYPE lit = cl_lits[j];
      ULINT var = GetVarFromLit(lit);
      if (IsLitNegated(lit)) {
        if (assignment[var] == -1) { num_true++; }
      } else {
        if (assignment[var] == 1) { num_true++; }
      }
      if (num_true) { break; }
    }
    if (!num_true) { quality += cl_weight; }
  }
  if (quality <= test_quality)
    cout << "passed." << endl;
  else {
    cout << "ERROR: quality=" << quality << ", but should be <= " << test_quality << endl;
    exit(-1);
  }
}


/** Dumps the clauses inside ubcsat; status = 0 means falsified only, 1 = satisfied
 * only, 2 = doesn't matter, 3 = don't look at the truth value. Note that as opposed
 * to _check_invariants() this method uses ubcsat's truth values. So, make sure
 * they are available before you call it with (status = 0, 1, 2).
 */
void Ubcsat12SLSWrapper::_dump_clauses(ostream& out, int status)
{
  out << "+Ubcsat12SLSWrapper::_dump_clauses(): " 
      << ((status == 0) ? "false" : ((status == 1) ? "true" : ""))
      << " clauses: " << endl;
  for (unsigned i = 0; i < _cl_lengths.size(); i++) {
    UINT32 cl_len = _cl_lengths[i];
    LITTYPE* cl_lits = _cl_lits[i];
    UBIGINT cl_weight = _cl_weights[i];
    bool sat = false;
    if (status != 3) {
      for (unsigned j = 0; j < cl_len; j++) {
        sat = IsLitTrue(cl_lits[j]);
        if (sat) { break; }
      } 
    }
    if ((status >= 2) || (status == sat)) {
      out << "  ";
      for (unsigned j = 0; j < cl_len; j++) {
        LITTYPE lit = cl_lits[j];
        ULINT var = GetVarFromLit(lit);
        out << (IsLitNegated(lit) ? "-" : "") << var << " ";
      }
      out << "0 (w=" << cl_weight;
      if (status < 3) { out << ", tv=" << sat; }
      out << ")" << endl;
    }
  }
  out << "-Ubcsat12SLSWrapper::_dump_clauses()." << endl;
}

// local implementations

namespace {

}
