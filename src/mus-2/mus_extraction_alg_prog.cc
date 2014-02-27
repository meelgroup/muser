/*----------------------------------------------------------------------------*\
 * File:        mus_extraction_alg_prog.cc
 *
 * Description: Implementation of the progression-based MUS extraction algorithm.
 *
 * Author:      antonb
 * 
 * Notes:       1. MULTI_THREADED features are removed completely; clears the
 *              lists inside MUSData. 
 *
 *
 *                                              Copyright (c) 2012, Anton Belov
\*----------------------------------------------------------------------------*/

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <iostream>
#include <sstream>
#include "basic_group_set.hh"
#include "mus_extraction_alg_prog.hh"
#include "utils.hh"

using namespace std;
//using namespace __gnu_cxx;

//#define DBG(x) x
//#define CHK(x) x

namespace {

}

/* The main extraction logic is implemented here. As usual the method does 
 * not modify the group set, but rather computes the group ids of MUS groups
 * in MUSData
 */
void MUSExtractionAlgProg::operator()(void)
{
  // TODO: replace param1 with a proper option
  if (config.get_verbosity() >= 2) {
    cout_pref << "wrkr-" << _id << " starting progression-based algorithm with ";
    switch(config.get_param1()) {
    case 1: cout << "simple linear scan" << endl; break;
    case 2: cout << "binary search on false clauses" << endl; break;
    case 3: cout << "linear scan on false clauses" << endl; break;
    case 0: default: config.set_param1(0); cout << "simple binary search" << endl;
    }
  }

  init_data();

  // main loop
  // loop inv: p_unknown <= p_removed
  //           [0,p_removed) \in \UNSAT
  //           [0,p_unknown) \in \bigcap \MUS([0,p_removed))
  for (size_t target_size = 1; (_p_unknown < _p_removed); ) {
    DBG(dump_state();); CHK(check_inv1(););
    auto p_target = _p_removed - min(target_size, (size_t)(_p_removed - _p_unknown));
    if (config.get_verbosity() >= 3) {
      cout_pref << "[" << RUSAGE::read_cpu_time() << " sec] main loop: " 
                << "nec = " << (_p_unknown - _all_gids.begin())
                << ", unn = " << (_all_gids.end() - _p_removed)
                << ", unk = " << (_p_removed - _p_unknown) 
                << ", target size " << target_size << endl;
    }
    if (check_range_status(p_target)) { // SAT
      DBG(cout << "  status: SAT, analyzing target set." << endl;);
      analyze_target_groups(p_target);
      target_size = 1;
      ++_prog_sat_outcomes;
      // TODO: with model rotation here we might have already an MUS, even if
      // p_unknown < p_removed ! should we add a test ?
    } else { // UNSAT - drop clauses and refine (if asked for)
      DBG(cout << "  status: UNSAT, dropping target groups" << endl;);
      for_each(p_target, _p_removed, [&](GID gid) { _md.mark_removed(gid); });
      _p_removed = p_target;
      _dropped_targets_prog += target_size;
      if (config.get_verbosity() >= 3)
        cout_pref << "removed " << target_size << " target groups." << endl;
      if (config.get_mus_mode() && config.get_refine_clset_mode()) 
        do_refinement(_p_unknown, _crs.unnec_gids());
      target_size <<= 1;
      ++_prog_unsat_outcomes;
    }
  } // main loop
  assert((_p_unknown == _p_removed) && "main loop should not be exited otherwise");
  CHK(check_inv2());

  // all done
  _sat_calls = _schecker.sat_calls();
  _sat_time = _schecker.sat_time();
  if (config.get_verbosity() >= 2) {
    cout_pref << "wrkr-" << _id << " finished, statistics: " << endl;
    cout_pref << " SAT calls: " << _sat_calls << endl;
    cout_pref << " SAT time: " << _sat_time << " sec" << endl;
    cout_pref << " SAT outcomes: " << _sat_outcomes << endl;
    cout_pref << " UNSAT outcomes: " << _unsat_outcomes << endl;
    cout_pref << " SAT calls by progression: " << _prog_sat_outcomes + _prog_unsat_outcomes << endl;
    cout_pref << " SAT outcomes in progression: " << _prog_sat_outcomes << endl;
    cout_pref << " UNSAT outcomes in progression: " << _prog_unsat_outcomes << endl;
    cout_pref << " targets dropped by progression: " << _dropped_targets_prog << endl;
    cout_pref << " targets dropped during search: " << _dropped_targets_search << endl;
    cout_pref << " refined groups: " << _ref_groups << endl;
    cout_pref << " rotated groups: " << _rot_groups << endl;
  }
}

// main functionality

/** Analyzes a set of target groups in the interval [p_target,p_removed). As 
 * a result at least one clause is added to the MUS.
 * @pre ([0,p_target) \in \SAT) && (_last_model is a model of [0,p_target) if doing MR)
 * @post (p_unknown' > p_unknown) && (p_removed' <= p_removed)
 */
void MUSExtractionAlgProg::analyze_target_groups(vector<GID>::iterator p_target)
{
  switch(config.get_param1()) {
  case 0: atg_binary_simple(p_target); break;
  case 1: atg_linear_simple(p_target); break;
  case 2: atg_binary_simple(p_target, true); break;      
  case 3: atg_linear_simple(p_target, true); break;
  }
}

/** Analyzes a set of target groups in the interval [p_target,p_removed) using
 * simple linear scan. All target clauses are considered, unless false_only = true
 * @pre ([0,p_target) \in \SAT) && (_last_model is a model of [0,p_target) if doing MR)
 * @post (p_unknown' > p_unknown) && (p_removed' <= p_removed)
 */
void MUSExtractionAlgProg::atg_linear_simple(vector<GID>::iterator p_target, bool false_only)
{
  DBG(cout << "Starting linear simple scan" << ((false_only) ? " false only" : "") << endl;);
  if (false_only) { shift_false_clauses(p_target); }
  for (auto p_curr = _p_removed - 1; p_curr != p_target; ) {
    DBG(cout << "  checking status of group " << *p_curr << endl;);
    if (check_range_status(p_curr)) { // SAT 
      DBG(cout << "  necessary, breaking out" << endl;);
      p_target = p_curr;
    } else { // UNSAT - drop p_curr
      DBG(cout << "  unnecessary, dropping" << endl;);
      _md.mark_removed(*p_curr);
      ++_dropped_targets_search;
      _p_removed = p_curr;
      if (config.get_mus_mode() && config.get_refine_clset_mode()) {
        // to maintain the indexes correctly we need to refine in stages (see
        // atg_binary_simple for the explanation)
        do_refinement(p_target, _crs.unnec_gids());
        p_target -= do_refinement(_p_unknown, _crs.unnec_gids(), false);
      }
      p_curr = _p_removed - 1;
    }
  }
  // p_target points to the necessary clause; p_last_model points to witness
  DBG(cout << "  found necessary group " << *p_target << endl;);
  swap(*_p_unknown, *p_target);
  _md.mark_necessary(*_p_unknown);
  ++_p_unknown;
  if (config.get_model_rotate_mode())
    do_model_rotation(_p_unknown-1, _last_model);
}


/** Analyzes a set of target groups in the interval [p_target,p_removed) using
 * binary search. All target clauses are considered.
 * @pre ([0,p_target) \in \SAT) && (model is a model of [0,p_target) if doing MR)
 * @post (p_unknown' > p_unknown) && (p_removed' <= p_removed)
 */
void MUSExtractionAlgProg::atg_binary_simple(vector<GID>::iterator p_target, bool false_only)
{
  DBG(cout << "Starting binary simple search" << ((false_only) ? " false only" : "") << endl;);
  if (false_only) { shift_false_clauses(p_target); }
  while (p_target < _p_removed - 1) {
    auto p_mid = p_target + (_p_removed - p_target)/2;
    DBG(cout << "  p_target=" << (p_target-_p_unknown) << ", p_mid=" 
        << (p_mid-_p_unknown) << ", p_removed=" << (_p_removed-_p_unknown) << endl;);
    if (check_range_status(p_mid)) { // SAT 
      DBG(cout << "  SAT, taking right half" << endl;);
      p_target = p_mid;
    } else { // UNSAT - drop p_curr
      DBG(cout << "  UNSAT, dropping right half" << endl;);
      for_each(p_mid, _p_removed, [&](GID gid) { _md.mark_removed(gid); });
      _dropped_targets_search += (_p_removed - p_mid);
      _p_removed = p_mid;
      if (config.get_mus_mode() && config.get_refine_clset_mode()) {
        // to maintain the indexes correctly we need to refine in stages: first 
        // the target clauses only - this will not affect p_target value, then the 
        // rest - this will; note that its important to preserve the order in the 
        // second round of refinement, so that target clauses are not moved away
        do_refinement(p_target, _crs.unnec_gids());
        p_target -= do_refinement(_p_unknown, _crs.unnec_gids(), false);
      }
    }
    DBG(cout << "  p_target=" << (p_target-_p_unknown) << ", p_mid=" 
        << (p_mid-_p_unknown) << ", p_removed=" << (_p_removed-_p_unknown) << endl;);

  }
  assert((p_target == _p_removed - 1) && "the loop should not be exited otherwise");
  // p_target points to the necessary clause; _last_model points to witness (assuming MR)
  DBG(cout << "  found necessary group " << *p_target << endl;);
  swap(*_p_unknown, *p_target);
  _md.mark_necessary(*_p_unknown);
  ++_p_unknown;
  if (config.get_model_rotate_mode())
    do_model_rotation(_p_unknown-1, _last_model);
}

// utilities

/** Prepares the relevant data fields 
 */
void MUSExtractionAlgProg::init_data(void)
{
  _save_model = config.get_model_rotate_mode() || (config.get_param1() == 2) 
    || (config.get_param1() == 3);
      
  // initialize the vector - as per the schedulers
  for (GID gid; _sched.next_group(gid, _id); _all_gids.push_back(gid));
  //reverse(_all_gids.begin(), _all_gids.end()); // only for convenience of experiments
  _p_unknown = _all_gids.begin(); // start of unknown; everything before is MUS
  _p_removed = _all_gids.end(); // start of removed; everything after is gone

  _crs.set_refine(config.get_mus_mode() && config.get_refine_clset_mode());
  _crs.set_need_model(_save_model, &_last_model);
  _crs.set_add_negation(config.get_irr_mode());

  _rm.set_ignore_g0(config.get_ig0_mode());
  _rm.set_ignore_global(config.get_iglob_mode());
  _rm.set_rot_depth(config.get_rotation_depth());
  _rm.set_rot_width(config.get_rotation_width());
}

/** Calls the SAT solvers to check the status of the range [0, p_range). 
 * @post if SAT and model rotation is on, _last_model will have the model
 * @post if UNSAT and refinement is on, _crs will have unnecessary GIDs
 */
bool MUSExtractionAlgProg::check_range_status(vector<GID>::iterator p_range)
{
  _crs.set_need_model(false); // to avoid wiping out last_model
  _crs.reset();
  _crs.set_need_model(_save_model, &_last_model);
  _crs.set_begin(_p_unknown);
  _crs.set_end(p_range);
  _crs.set_allend(_p_removed);
  DBG(cout << "  testing target set: { "; 
      copy(p_range, _p_removed, ostream_iterator<GID>(cout, " ")); cout << "}" << endl;);
  _schecker.process(_crs);
  _md.clear_lists(); 
  if (!_crs.completed()) // TODO: handle this properly
    tool_abort(string("could not complete SAT check; in ")+__PRETTY_FUNCTION__);
  ++(_crs.status() ? _sat_outcomes : _unsat_outcomes);
  return _crs.status();
}

/** Runs the model rotation algorithm
 * @pre p_nec < p_unknown points to a group necessary for [0,p_removed)
 *      model is its witness
 * @post p_unknown' >= p_unknown
 */
void MUSExtractionAlgProg::do_model_rotation(vector<GID>::iterator p_nec, IntVector& model)
{
  _rm.set_gid(*p_nec);
  _rm.set_model(model);
  _mrotter.process(_rm);
  if (!_rm.completed())
    tool_abort(string("could not complete model rotation; in ")+__PRETTY_FUNCTION__);
  GIDSet& nec_gids = _rm.nec_gids();
  DBG(cout << "  model rotation: " << nec_gids.size() << " necessary groups (may incl MUS)" << endl;);
  // move all necessary groups to the front
  auto p_border = partition(_p_unknown, _p_removed, 
                            [&](GID gid) { return nec_gids.count(gid); });
  for_each(_p_unknown, p_border, [&](GID gid) { _md.mark_necessary(gid); });
  if (config.get_verbosity() >= 3)
    cout_pref << (p_border - _p_unknown) << " necessary groups due to model rotation" << endl;
  _rot_groups += (p_border - _p_unknown);
  _p_unknown = p_border;
  _rm.reset();
}

/** Refines the set [p_from, p_removed) by dropping all groups from the range 
 * that appear in unnec_gids, i.e. all these groups are shifted to past p_removed
 * @param fast - if true, the remaining groups might be reshuffled (faster), 
 *               otherwise the order is preserved.
 * @pre p_from < p_removed
 * @return r = number of groups removed from the range
 * @post p_removed' == p_removed - r
 */
size_t MUSExtractionAlgProg::do_refinement(vector<GID>::iterator p_from, 
                                           const GIDSet& unnec_gids,
                                           bool fast)
{
  DBG(cout << "  refinement: " << unnec_gids.size() << " unnecessary groups: " 
      << unnec_gids << endl;);
  vector<GID>::iterator p_border;
  if (fast)
    p_border = partition(p_from, _p_removed, [&](GID gid) { return !unnec_gids.count(gid); });
  else 
    p_border = stable_partition(p_from, _p_removed, [&](GID gid) { return !unnec_gids.count(gid); });
  for_each(p_border, _p_removed, [&](GID gid) { _md.mark_removed(gid); });
  size_t r = _p_removed - p_border;
  _p_removed = p_border;
  _ref_groups += r;
  if (config.get_verbosity() >= 3)
    cout_pref << "removed " << r << " additional groups with refinement." << endl;
  return r;
}

/** Shifts falsified groups in the range [p_target, p_removed) to the end of the 
 * range, and adjusts p_target to point to the begining of false clauses.
 * @pre ([0,p_target) \in \SAT) && (_last_model is a model of [0,p_target))
 * @post ([0,p_target') \in \SAT) && (model is a model of [0,p_target') && 
 *       the groups [p_target',p_removed) are false under model
 */
void MUSExtractionAlgProg::shift_false_clauses(vector<GID>::iterator& p_target)
{
  p_target = partition(p_target, _p_removed, [&](GID gid) { 
      return Utils::tv_group(_last_model, _md.gset().gclauses(gid)) == 1; });
}

// debugging

/** Prints out the current state
 */
void MUSExtractionAlgProg::dump_state(void) 
{
  auto p_begin = _all_gids.begin();
  auto p_end = _all_gids.end();
  cout << "Current state: " << (_p_unknown - p_begin) << " MUS groups, "
       << (_p_removed - _p_unknown) << " unknown groups, " 
       << (p_end - _p_removed) << " removed groups." << endl;
  cout << "| ";
  copy(p_begin, _p_unknown, ostream_iterator<GID>(cout, " "));
  cout << "| ";
  if (_p_removed - _p_unknown > 100) {
    copy(_p_unknown, _p_unknown + 100, ostream_iterator<GID>(cout, " "));
    cout << " ... ";
  } else 
    copy(_p_unknown, _p_removed, ostream_iterator<GID>(cout, " "));
  cout << "| ";
  if (p_end - _p_removed > 100) {
    copy(_p_removed,  _p_removed + 100, ostream_iterator<GID>(cout, " "));
    cout << " ... ";
  } else 
    copy(_p_removed,  p_end, ostream_iterator<GID>(cout, " "));
  cout << "|" << endl;
}

/** Checks the main loop body invariant
 *  loop inv: p_unknown < p_removed
 *            [0,p_removed) \in \UNSAT
 *            [0,p_unknown) are necessary
 */
void MUSExtractionAlgProg::check_inv1(void)
{
  cout << "WARNING: expensive invariant check (loop body)." << endl;
  bool passed = true;
  if (_p_unknown >= _p_removed) {
    cout << "ERROR: p_unknown > p_removed" << endl;
    passed = false;
  }
  if (check_range_status(_p_removed)) {
    cout << "ERROR: [0,p_removed) is SAT" << endl;
    passed = false;
  }
  for (auto pg = _all_gids.begin(); pg < _p_unknown; ++pg) {
    if (!_md.nec(*pg)) {
      cout << "ERROR: group " << *pg << " is inside [0,p_unknown) but not marked necessary" << endl;
      passed = false;
      break;
    }
  }
  if (!passed) {
    cout << "ERROR: invariant test failed, terminating" << endl; exit(-1);
  } else {
    cout << "INFO: invariant check PASSED." << endl;
  }
}

/** Checks the main loop termination invariant
 *  loop inv: p_unknown == p_removed
 *            [0,p_unknown) \in \UNSAT
 *            [0,p_unknown) are necessary
 */
void MUSExtractionAlgProg::check_inv2(void)
{
  cout << "WARNING: expensive invariant check (loop termination). " << endl;
  bool passed = true;
  if (_p_unknown != _p_removed) {
    cout << "ERROR: p_unknown != p_removed" << endl;
    passed = false;
  }
  if (check_range_status(_p_unknown)) {
    cout << "ERROR: [0,p_unknown) is SAT" << endl;
    passed = false;
  }
  for (auto pg = _all_gids.begin(); pg < _p_unknown; ++pg) {
    if (!_md.nec(*pg)) {
      cout << "ERROR: group " << *pg << " is inside [0,p_unknown) but not marked necessary" << endl;
      passed = false;
      break;
    }
  }
  if (!passed) {
    cout << "ERROR: invariant test failed, terminating" << endl; exit(-1);
  } else {
    cout << "INFO: invariant check PASSED." << endl;
  }
}

// local implementations ....

namespace {

}


/*----------------------------------------------------------------------------*/


