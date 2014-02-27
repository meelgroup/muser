/*----------------------------------------------------------------------------*\
 * File:        mus_extraction_alg_fbar.cc
 *
 * Description: Implementation of the GMUS extraction algorithm specialized for
 *              flop-based abstraction-refinement instances.
 *
 * Author:      antonb
 * 
 * Notes:
 *
 *
 *                                              Copyright (c) 2012, Anton Belov
\*----------------------------------------------------------------------------*/

#include <cassert>
#include <cstdio>
#include <iostream>
#include <sstream>
#include "basic_group_set.hh"
#include "mus_extraction_alg_fbar.hh"
#include "rgraph_utils.hh"
#include "utils.hh"

using namespace std;

//#define DBG(x) x
//#define CHK(x) x

namespace {

}

/** The main extraction logic is implemented here. As usual the method does
 * not modify the group set, but rather computes the group ids of MUS groups
 * in MUSData
 */
void MUSExtractionAlgFBAR::operator()(void)
{
  DBG(cout << "+MUSExtractionAlgFBAR: started ..." << endl;);

  // get parameters
  _cleanup_online = config.get_param1() & 1;
  _cleanup_after = config.get_param1() & 2;
  _skip_witnesses = config.get_param1() & 4;
  _skip_rcheck = config.get_param1() & 8;
  _use_rgraph = config.get_param1() & 16;
  _set_phase = config.get_param1() & 32;
  _skip_insertion = config.get_param2();
  _cegar = config.get_param3();
  if (config.get_verbosity() >= 2) {
    cout_pref << "FBAR algorithm parameters: " 
              << " cleanup_online=" << _cleanup_online
              << " cleanup_after=" << _cleanup_after
              << " skip_witnesses=" << _skip_witnesses
              << " skip_rcheck=" << _skip_rcheck
              << " use_rgraph=" << _use_rgraph
              << " set_phase=" << _set_phase
              << " skip_insertion=" << _skip_insertion
              << " cegar=" << _cegar;
      ;
    if (!_skip_witnesses && _skip_rcheck)
      cout << " WARNING: !skip_witnesses conflicts with skip_rcheck";
    if (_set_phase && !_use_rgraph)
      cout << " WARNING: set_phase has no effect without use_rgraph";
    cout << endl;
  }

  _init_data();

  _untrimmed = (_solver.gsize() == 0);

  // load group-set, if needed
  if (_untrimmed) {
    if (_cegar) {
      _solver.add_group(_md.gset(), 0);
      if (config.get_verbosity() >= 3) { cout_pref << " incremental CEGAR." << endl; }
    } else {
      _solver.add_groups(_md.gset());
    }
    DBG(cout << "loaded group-set" << endl;);
  }

  if (_cegar || !_skip_insertion) {
    // disable all and hit group 0
    DBG(cout << "testing group 0 ..." << endl;);
    if (!(_cegar && _untrimmed))
      for (GID gid : _untested_gids) { _solver.deactivate_group(gid); }
    SATRes outcome = _solve();
    if (outcome == SAT_False) {
      // group 0 is UNSAT, we're done
      _md.make_empty_gmus();
      if (config.get_verbosity() >= 3)
        cout_pref << "wrkr-" << _id << " gid 0 is UNSAT, finishing up." << endl;
      goto _done;
    }
    DBG(cout << "group 0 is SAT." << endl;);
  }
  if (_cegar) {
    _do_cegar();
    if (config.get_verbosity() >= 2) { prt_cfg_cputime("CEGAR finished at "); }
    if (config.get_verbosity() >= 3)
      cout_pref << "wrkr-" << _id << " : " << _cand_gids.size() 
                << " candidates after CEGAR." << endl;
  } else if (!_skip_insertion) {
    _do_insertion();
    if (config.get_verbosity() >= 2) { prt_cfg_cputime("Insertion loop finished at "); }
    if (config.get_verbosity() >= 3)
      cout_pref << "wrkr-" << _id << " : " << _cand_gids.size() 
                << " candidates after insertion loop." << endl;
  } else {
    _cand_gids = _untested_gids;
    _untested_gids.clear();
  }
  if (_cleanup_after) 
    _cleanup_cands();

  // all done
  for (GID gid : _cand_gids) { _md.mark_necessary(gid); } 

 _done:
  _solver.reset_run();
  _schecker.sync_solver(_md); // sync the results (in case we'll use the solver)
  _reset_data();
  if (config.get_verbosity() >= 2) {
    cout_pref << "wrkr-" << _id << " finished; "
              << " SAT calls: " << _sat_calls
              << ", SAT time: " << _sat_time << " sec" 
              << ", SAT outcomes: " << _sat_outcomes
              << ", UNSAT outcomes: " << _unsat_outcomes
              << ", SAT time SAT: " << _sat_time_sat
              << " sec (" << (_sat_time_sat/_sat_outcomes) << " sec/call)"
              << ", SAT time UNSAT: " << _sat_time_unsat
              << " sec (" << (_sat_time_unsat/_unsat_outcomes) << " sec/call)"
              << endl;
  }
}

// private stuff ...

/** Performs initial over-approximation using "CEGAR"; assumes the SAT
 * solver just finished solving the current set and the outcome was SAT_True,
 * and all groups inside _untested_gids are deactivated.
 */
void MUSExtractionAlgFBAR::_do_cegar(void)
{
  DBG(cout << "  = doing CEGAR" << endl);
  while (1) {
    unsigned unsat = false; // only for assertion testing
    DBG(cout << "  UNSAT groups after SAT outcome: ");
    if (_cegar == 1) {
      for (auto pg = _untested_gids.begin(); pg != _untested_gids.end(); ) {
        GID gid = *pg;
        if (!_satisfies_group(gid, _solver.get_model())) {
          unsat = true;
          if (_untrimmed)
            _solver.add_group(_md.gset(), gid);
          else
            _solver.activate_group(gid);
          _cand_gids.insert(gid);
          pg = _untested_gids.erase(pg);
          DBG(cout << gid << " ");
        } else { ++pg; }
      }
    } 
    if (_cegar > 1) {
      typedef pair<GID, unsigned> gidp;
      vector<gidp> fg;
      for (GID gid : _untested_gids) {
        unsigned nf = _num_fclauses(gid, _solver.get_model());
        if (nf > 0) fg.push_back({ gid, nf });
        DBG(if (nf > 0) { cout << gid << "(" << nf << ") "; });
      }
      unsat = !fg.empty();
      if (fg.size() > 10) { // heuristic ?
        sort(fg.begin(), fg.end(), [&](const gidp& p1, const gidp& p2) { 
            return p1.second > p2.second; });
        fg.resize(fg.size() / _cegar);
      }
      DBG(cout << endl << "  Chosen for refinement: ";);
      for (gidp& p : fg) {
        if (_untrimmed)
          _solver.add_group(_md.gset(), p.first);
        else
          _solver.activate_group(p.first);
        _cand_gids.insert(p.first);
        _untested_gids.erase(p.first);
        DBG(cout << p.first << "(" << p.second << ") ";);
      }
    }
    DBG(cout << endl;);
    assert(unsat && "the approximation must be UNSAT here, o/w the whole thing is SAT");
    if (config.get_verbosity() >= 3)
      cout_pref << "wrkr-" << _id << " num candidates: " << _cand_gids.size() << endl;
    if (_solve() == SAT_False)
      break;
  }
  // ok, got UNSAT, drop all the rest and everything not in the core
  DBG(cout << "  got UNSAT, removing " << _untested_gids.size() << " remaining groups." << endl;);
  _ref_groups += _untested_gids.size();
  for (auto pg = _untested_gids.begin(); pg != _untested_gids.end(); 
       pg = _untested_gids.erase(pg)) {
    if (!_untrimmed) // for _untrimmed, its not even there
      _solver.del_group(*pg);
    _md.mark_removed(*pg);
  }
  unsigned curr_size = _cand_gids.size();
  GIDSet& gcore = _solver.get_group_unsat_core();  
  for (auto pg = _cand_gids.begin(); pg != _cand_gids.end(); ) {
    if (!gcore.count(*pg)) {
      _solver.del_group(*pg);
      _md.mark_removed(*pg);
      pg = _cand_gids.erase(pg);
    } else { ++pg; }
  }
  curr_size -= _cand_gids.size();
  _ref_groups += curr_size;
  DBG(cout << "  removed " << curr_size << " with refinement" << endl;);
  DBG(cout << "  =finished CEGAR" << endl;);
  return;
}

/** Performs the insertion loop. Assumes the SAT solver just finished solving 
 * the current set and the outcome was SAT_True, and all groups inside 
 * _untested_gids are deactivated.
 */
void MUSExtractionAlgFBAR::_do_insertion(void)
{
  // main loop
  DBG(cout << "  =insertion loop" << endl;);
  SATRes outcome = SAT_True;
  GID gid = gid_Undef;
  while ((gid = _pick_next_group(gid, outcome)) != gid_Undef) {
    DBG(cout << "  checking the status of group " << gid << endl;);
    if (!_skip_rcheck) { _add_neg_group(gid); }
    outcome = _solve();
    if (!_skip_rcheck) { _remove_neg_group(); }
    if (outcome == SAT_False) { // group is redundant, remove permanently
      _solver.del_group(gid);
      _md.mark_removed(gid);
      if (config.get_verbosity() >= 3)
        cout_pref << "wrkr-" << _id << " group " << gid << " is redundant, removed." << endl;
      continue;
    }
    DBG(cout << "  group is irredundant" << endl;);
    // group is irredundant, add it, but need to check whether it made some 
    // others redundant
    _cand_gids.insert(gid);
    _solver.activate_group(gid);
    if (!_skip_witnesses) { _store_witness(gid, _solver.get_model()); }
    if (_cleanup_online) {
      _cleanup_cands(gid);
      CHK(if (!_skip_witnesses) _check_invariants(););
    }
  }
}

/** Removes all groups made redundant by the addition of gid to cand_gids.
 * Assumes that the witnesses for all cand_gids are already stored.
 */
void MUSExtractionAlgFBAR::_cleanup_cands(GID gid)
{
  DBG(cout << "  =cleaning up using gid " << gid << endl;);
  for (auto pg = _cand_gids.begin(); pg != _cand_gids.end(); ) {
    GID cand_gid = *pg;
    bool cheap_fix = false;
    if (cand_gid != gid) {
      if (!_skip_witnesses) {
        IntVector& witness = _get_witness(cand_gid);
        if (!(cheap_fix = _satisfies_group(gid, witness))) {
          DBG(cout << "  witness is NOT ok for " << cand_gid << ", trying to fix." << endl;);
          cheap_fix = _try_fix_witness(cand_gid, witness, gid);
          DBG(cout << "  witness is " << (cheap_fix ? "" : "NOT") << " fixed for " 
              << cand_gid << endl;);
        }
      }
      if (!cheap_fix) {
        DBG(cout << "  retesting gid " << cand_gid << endl;);
        _solver.deactivate_group(cand_gid);
        if (!_skip_rcheck) { _add_neg_group(cand_gid); }
        SATRes outcome = _solve();
        if (!_skip_rcheck) { _remove_neg_group(); }
        if (outcome == SAT_False) { // group became redundant, remove
          pg = _cand_gids.erase(pg);
          _solver.del_group(cand_gid);
          if (!_skip_witnesses) { _remove_witness(cand_gid); }
          _md.mark_removed(cand_gid);
          if (config.get_verbosity() >= 3)
            cout_pref << "wrkr-" << _id << " group " << cand_gid 
                      << " became redundant, removed." << endl;
          continue;
        }
        // group is still irredundant -- store new witness
        DBG(cout << "  " << cand_gid << " is still irredundant." << endl;);
        _solver.activate_group(cand_gid);
        _store_witness(cand_gid, _solver.get_model());
      }
    }
    ++pg;
  }
  DBG(cout << "  =finished cleanup" << endl);
}


/** Removes all redundant groups from cand_gids. Asumes that the witnesses 
 * for all cand_gids are already stored, and that the set is UNSAT.
 */
void MUSExtractionAlgFBAR::_cleanup_cands(void)
{
  DBG(cout << "  =cleaning up completely" << endl;);
  assert(_untested_gids.empty() && "there should be no untested groups");
  _untested_gids = _cand_gids;
  _cand_gids.clear();
  // main loop
  SATRes outcome = SAT_False;
  GID gid = gid_Undef;
  while ((gid = _pick_next_group(gid, outcome)) != gid_Undef) {
    DBG(cout << "checking the status of group " << gid << endl;);
    bool cheap_fix = false;
    if (!_skip_witnesses) {
      IntVector& witness = _get_witness(gid);
      cheap_fix = _is_witness(gid, witness);
      DBG(cout << "  witness is " << (cheap_fix ? "" : "NOT") << " ok for " 
          << gid << endl;);
    }
    if (!cheap_fix) {
      DBG(cout << "  testing gid " << gid << endl;);
      _solver.deactivate_group(gid);
      outcome = _solve();
      if (outcome == SAT_False) { // group became redundant
        _untested_gids.insert(gid); // only to be removed below
        if (config.get_verbosity() >= 3)
          cout_pref << "wrkr-" << _id << " group " << gid
                    << " became redundant, removed." << endl;
        GIDSet& gcore = _solver.get_group_unsat_core();
        unsigned curr_size = _untested_gids.size();
        for (auto pg = _untested_gids.begin(); pg != _untested_gids.end(); ) {
          if (!gcore.count(*pg)) {
            _solver.del_group(*pg);
            if (!_skip_witnesses) { _remove_witness(*pg); }
            _md.mark_removed(*pg);
            pg = _untested_gids.erase(pg);
          } else
            ++pg; 
        }
        curr_size -= _untested_gids.size();
        _ref_groups += curr_size;
        if (config.get_verbosity() >= 3)
          cout_pref << "wrkr-" << _id << " " << curr_size 
                    << " groups removed with refinement" << endl;
        continue; 
      }
      _solver.activate_group(gid);
    }
    DBG(cout << "  " << gid << " is still irredundant." << endl;);
    _solver.make_group_final(gid);
    _cand_gids.insert(gid);
  }
  DBG(cout << "  =finished cleanup" << endl);
}


/** Initializes all of the internal data. */
void MUSExtractionAlgFBAR::_init_data(void)
{
  _untested_gids.clear();
  _cand_gids.clear();
  BasicGroupSet& gs = _md.gset();
  for (gset_iterator pg = gs.gbegin(); pg != gs.gend(); ++pg)
    if (*pg && _md.untested(*pg)) { _untested_gids.insert(*pg); }
  _w_map.clear();
}

/** Cleans up all of the internal data */
void MUSExtractionAlgFBAR::_reset_data(void)
{
  _untested_gids.clear();
  _cand_gids.clear();
  _w_map.clear();
  _neg_ass.clear();
  _assumps.clear();
}

/** Invokes the SAT solver (and takes care of stats). Uses assumptions from the
 * _assumps vector for the call (does not remove them !). Ensures the outcome 
 * is valid.
 */
SATRes MUSExtractionAlgFBAR::_solve(void)
{
  try { _solver.reset_run(); } catch(...) {}
  _solver.init_run();
  start_sat_timer();
  SATRes outcome = _solver.solve(_assumps);
  stop_sat_timer(outcome);
  _sat_calls++;
  if (outcome == SAT_True) { ++_sat_outcomes; }
  if (outcome == SAT_False) { ++_unsat_outcomes; }
  if ((outcome != SAT_True) && (outcome != SAT_False)) 
    tool_abort("Unexpected outcome from SAT solver in FBAR algorithm");
  return outcome;
}

/** Returns next group to analyze or gid_Undef. Assumes that the witness
 * of last_gid is available. */
GID MUSExtractionAlgFBAR::_pick_next_group(GID last_gid, SATRes last_outcome)
{
  // quick test for termination
  if (_untested_gids.empty()) { return gid_Undef; }
  
  // seed
  if ((last_gid == gid_Undef) && (last_outcome == SAT_True)) { // seed
    // pick the "seed" group -- the first one falsified by the g0's model
    for (auto pg = _untested_gids.rbegin(); pg != _untested_gids.rend(); ++pg) {
      GID seed_gid = *pg;
      if (!_satisfies_group(seed_gid, _solver.get_model())) {
        DBG(cout << "found the seed group " << seed_gid << endl;);
        if (!_skip_witnesses) { _store_witness(seed_gid, _solver.get_model()); }
        _cand_gids.insert(seed_gid);
        _untested_gids.erase(seed_gid);
        return seed_gid;
      }
    }
    tool_abort("SAT instance is given to FBAR algorithm.");
  }

  // main logic
  GID res = gid_Undef;
  if ((last_outcome == SAT_True) && _use_rgraph) {
    DBG(cout << "  using resolution graph heuristic ... " << flush;);
    BasicClauseVector fcls;
    const BasicGroupSet& gs = _md.gset();
    for (BasicClause* cl : gs.gclauses(last_gid))
      if (!cl->removed() && Utils::tv_clause(_solver.get_model(), cl) == -1)
          fcls.push_back(cl);
    if (!fcls.empty()) {
      vector<ULINT> path;
      BasicClause* target = RGraphUtils::find_target(gs, fcls, _untested_gids, 
                                                     true, 10000, false, 
                                                     (_set_phase ? &path : 0));
      if (target) {
        res = target->get_grp_id();
        DBG(cout << " found target, group ID = " << target->get_grp_id() << endl;);
        if (_set_phase) {
          const IntVector& model = _solver.get_model();
          for (ULINT var : path)
            _solver.set_phase(var, model[var] == 1 ? 0 : 1);
        }
      } DBG(else { cout << " couldn't find target, falling back." << endl; });
    } DBG(else { cout << " no false clauses, falling back." << endl; });
  }
  // fall back ...
  if (res == gid_Undef) { res = *_untested_gids.rbegin(); }
  // done
  if (res != gid_Undef) { _untested_gids.erase(res); }
  return res;
}

/** Adds negation of the group gid to the solver. The vector _assumps
 * is populated with the assumptions for the next call; the vector
 * _neg_ass is populated with the literals than must be asserted to 
 * remove the negation of the group (if any).
 */
void MUSExtractionAlgFBAR::_add_neg_group(GID gid)
{
  BasicGroupSet& gs = _md.gset();
  const BasicClauseVector& cls = gs.gclauses(gid);
  _assumps.clear();
  if (cls.size() == 1) { // singleton, nothing to add, just assume
    const BasicClause* cl = *cls.begin();
    assert(!cl->removed() && "clause cannot be removed, as otherwise the whole group is");
    for_each(cl->abegin(), cl->aend(), [&](LINT lit) { _assumps.push_back(-lit); });
  } else {
    // for each clause Ci = (l1,...,lk) make a new auxiliary variable ai, and 
    // add the clauses (-ai,-l1) ... (-ai,-lk) to the solver. Then, add a clause
    // (a1,...an) to complete the transform.
    vector<LINT> lits(2, 0); // binary clauses
    _neg_ass.clear();
    for (const BasicClause* cl : cls) {
      if (cl->removed()) { continue; }
      ULINT aux_var = _imgr.new_id();      
      for_each(cl->abegin(), cl->aend(), [&](LINT lit) {
          lits[0] = -lit;
          lits[1] = -aux_var;
          BasicClause* new_cl = gs.make_clause(lits, 0);
          _solver.add_final_clause(new_cl);
          gs.destroy_clause(new_cl);
        });
      _neg_ass.push_back(aux_var);
    }
    lits = _neg_ass;
    ULINT aux_var = _imgr.new_id();     // assumption for the long clause
    lits.push_back(aux_var);    
    BasicClause* new_cl = gs.make_clause(lits, 0);
    _solver.add_final_clause(new_cl);
    gs.destroy_clause(new_cl);
    _assumps.push_back(-aux_var);
  }
}

/** Undoes _add_neg_group(). */
void MUSExtractionAlgFBAR::_remove_neg_group(void)
{
  if (!_neg_ass.empty()) { 
    assert(_assumps.size() == 1 && "should have a single assumption");
    // remove long clause and then the binary clauses
    _solver.add_final_unit_clause(-*_assumps.begin());
    for (LINT aux_lit : _neg_ass) { _solver.add_final_unit_clause(-aux_lit); }
    _neg_ass.clear();
  }
  _assumps.clear();
}

/** Stores or updates the witness for the specified group. */
void MUSExtractionAlgFBAR::_store_witness(GID gid, const IntVector& model)
{
  _w_map[gid] = model;
}

/** Returns the witness for the specified group.
 */
IntVector& MUSExtractionAlgFBAR::_get_witness(GID gid)
{
  return _w_map[gid];
}

/** Removes the witness for the specified group.
 */
void MUSExtractionAlgFBAR::_remove_witness(GID gid)
{
  _w_map.erase(gid);
}

/** Returns true if the assignment satisfies the given group; false otherwise */
bool MUSExtractionAlgFBAR::_satisfies_group(GID gid, const IntVector& ass)
{
  const BasicClauseVector& cls = _md.gset().gclauses(gid);
  for (const BasicClause* cl : cls) 
    if (!cl->removed() && (Utils::tv_clause(ass, cl) != 1))
      return false;
  return true;
}

/** Returns the number of clauses in the group falsified by the assignment */
unsigned MUSExtractionAlgFBAR::_num_fclauses(GID gid, const IntVector& ass)
{
  const BasicClauseVector& cls = _md.gset().gclauses(gid);
  unsigned count = 0;
  for (const BasicClause* cl : cls) 
    if (!cl->removed() && (Utils::tv_clause(ass, cl) != 1))
      count++;
  return count;
}

/** Attempts to fix the witness for cand_gid using some cheap heuristic, and
 * given that gid is the only other falsified group. Returns true if succeeded.
 */
bool MUSExtractionAlgFBAR::_try_fix_witness(GID cand_gid, IntVector& witness, GID gid)
{
  return false;
}

/** Tests wether a given assignment is a witness for a group (among _cand_gids 
 * and _untested_gids); does not check group 0 (so don't use for invariant checking).
 */
bool MUSExtractionAlgFBAR::_is_witness(GID gid, const IntVector& witness)
{
  if (_satisfies_group(gid, witness))
    return false;
  for (GID cand_gid : _cand_gids) {
    if (cand_gid == gid) { continue; }
    if (!_satisfies_group(cand_gid, witness))
      return false;
  }
  for (GID cand_gid : _untested_gids) {
    if (cand_gid == gid) { continue; }
    if (!_satisfies_group(cand_gid, witness))
      return false;
  }
  return true;
}

/** An expensive test of invariants:
 * 1. witness is indeed the assignment that falsifies only the associated group
 *    among the groups in _cand_gids
 */
void MUSExtractionAlgFBAR::_check_invariants(void)
{
  cout_pref << "WARNING: expensive invariants test; status: " << flush;
  const BasicGroupSet& gs = _md.gset();
  bool passed = true;
  for (auto pm : _w_map) {
    GID gid = pm.first;
    IntVector& witness = pm.second;
    GIDSet false_gids;
    GID test_gid = 0;
    auto pg = _cand_gids.begin();
    while (1) {
      const BasicClauseVector& cls = gs.gclauses(test_gid);
      for (const BasicClause* cl : cls) {
        if (cl->removed()) { continue; }
        if (Utils::tv_clause(witness, cl) != 1) {
          false_gids.insert(test_gid);
          break;
        }
      }
      if (test_gid != 0) { ++pg; }
      if (pg == _cand_gids.end()) { break; }
      test_gid = *pg;
    }
    if ((false_gids.size() != 1) || !false_gids.count(gid)) {
      cout << "ERROR -- witness of group " << gid << " falsifies groups " << false_gids << "; ";
      passed = false;
    }
  }
  if (!passed) { cout << endl; exit(0); }
  cout << "passed. " << endl;
}


// local implementations ....

namespace {

}

/*----------------------------------------------------------------------------*/


