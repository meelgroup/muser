/*----------------------------------------------------------------------------*\
 * File:        mus_extraction_alg_subset2.cc
 *
 * Description: Implementation of the subset-based deletion-based MUS 
 *              extraction algorithm; experimental version with resolution-graph
 *              based heuristics.
 *
 * Author:      antonb
 * 
 * Notes:       
 *                                              Copyright (c) 2012, Anton Belov
\*----------------------------------------------------------------------------*/

#include <cassert>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <queue>
#include "basic_group_set.hh"
#include "id_manager.hh"
#include "mus_extraction_alg.hh"
#include "solver_wrapper.hh"
#include "solver_factory.hh"

using namespace std;
using namespace __gnu_cxx;

//#define DBG(x) x

namespace {

  // Returns the truth-value of clause under assignment: -1;0:+1
  int tv_clause(const IntVector& ass, const BasicClause* cl);
  // Checks whether a given assignment satisfies a given group: -1;0;+1
  int tv_group(const IntVector& ass, const BasicGroupSet& gset, GID gid);

}

    
/* The main extraction logic is implemented here. As usual the method does 
 * not modify the group set, but rather computes the group ids of MUS groups
 * in MUSData;
 */
void MUSExtractionAlgSubset2::operator()(void)
{
  // +TEMP: this should extend to groups too, but not yet
  if (config.get_grp_mode())
    throw logic_error("MUSExtractionAlgSubset2: group mode is not yet supported");
  // -TEMP

  BasicGroupSet& gset = _md.gset();
  GIDSet subset_gids;  // the "subset"
  CheckSubsetStatus css(_md, subset_gids);
  css.set_refine(config.get_refine_clset_mode());
  css.set_need_model(true);
  css.set_use_rr(false); // TEMP
  RotateModel rm(_md); // item for model rotations
  rm.set_rot_depth(config.get_rotation_depth());
  rm.set_rot_width(config.get_rotation_width());
  rm.set_ignore_g0(config.get_ig0_mode());
  rm.set_ignore_global(config.get_iglob_mode());
  GIDSet single_mode_gids; // groups to be tested in single-group mode 
                           // (in case of non-singleton SAT)
  
  // main loop
  while (1) {
    bool expecting_unsat = false; // for stats
    // populate the current subset
    assert(subset_gids.empty());
    // if single_mode_gids is not empty, then we are in a single-group mode -- 
    // take the first untested one (if any) and make a singleton subset
    auto pgid = find_if(single_mode_gids.begin(), single_mode_gids.end(), 
                        [&](GID gid) { return _md.untested(gid); });
    if (pgid != single_mode_gids.end())
     subset_gids.insert(*pgid);
    single_mode_gids.erase(single_mode_gids.begin(), pgid);
    DBG(if (!subset_gids.empty())
          cout << "Working in the single-group mode, testing gid = " << *subset_gids.begin() << endl;);
    // if subset_gids is empty now, then need to make the next subset
    if (subset_gids.empty()) {
      switch (config.get_subset_mode()) {
      case 10: 
        make_order_subset(subset_gids); break;
      case 11:
        make_rgraph_1hood_subset(subset_gids); break;
      default:  
        throw logic_error("Unsupported subset mode");
      }
      // if subset_gids is empty now, then we're done
      if (subset_gids.empty()) {
        DBG(cout << "No more groups to test." << endl;);
        break;
      }
      _unsat_guesses++;
      expecting_unsat = true;
      DBG(cout << "Got next subset, size = " << subset_gids.size() << ": " 
          << subset_gids << endl;);
    }
    if (config.get_verbosity() >= 3)
      cout_pref << "wrkr-" << _id << " checking gid subset " << subset_gids << " ... " << endl;
    _schecker.process(css);
    if (!css.completed()) // TODO: handle this properly
      throw runtime_error("could not complete SAT check");
    if (config.get_verbosity() >= 3) {
      if (css.status())
        cout_pref << "wrkr-" << _id << " " << " some groups are necessary." << endl;
      else
        cout_pref << "wrkr-" << _id << " " << css.unnec_gids().size()
                  << " unnecessary groups." << endl;
    }
    if (css.status()) { // SAT
      // given the model, compute the set of falsified gids; if it is a singleton, 
      // then we have a new necessary group; otherwise - perpahs go into single-group
      // analysis mode to analyze each separately
      GIDSet false_gids;
      for (GID gid : subset_gids)
        if (tv_group(css.model(), gset, gid) == -1) // undetermined cannot be assumed unsat
          false_gids.insert(gid);
        else // put the true ones back
          _sched.reschedule(gid);
      DBG(cout << "SAT: falsified gids " << false_gids << " ";);
      if (single_mode_gids.empty())
        ++((false_gids.size() == 1) ? _subset_singleton : _subset_nonsingleton);
      if (false_gids.size() == 1) {
        DBG(cout << "got a singleton, adding, rotating if needed." << endl;);
        // take care of the necessary group: put into MUSData, and mark final
        GID gid = *false_gids.begin();
        _md.mark_necessary(gid);
        // do rotation, if asked for it
        if (config.get_model_rotate_mode()) {
          rm.set_gid(gid);
          rm.set_model(css.model());
          _mrotter.process(rm);
          if (rm.completed()) {
            unsigned r_count = 0;
            for (GID gid : rm.nec_gids()) {
              // double-check check if not necessary already and not gid 0
              if (gid && !_md.nec(gid)) {
                _md.mark_necessary(gid);
                r_count++;
              }
            }
            if ((config.get_verbosity() >= 3) && r_count)
              cout_pref << "wrkr-" << _id << " " << r_count
                        << " groups are necessary due to rotation." << endl;
            _rot_groups += r_count;
          }
          rm.reset();
        }
      } else {
        DBG(cout << "not a singleton, analyzing one by one." << endl;);
        assert(single_mode_gids.empty());
        single_mode_gids = false_gids;
      }
      ++_sat_outcomes;
    } else { // gsc.status = UNSAT
      // take care of unnecessary groups
      GIDSet& ugids = css.unnec_gids();
      DBG(cout << "UNSAT: " << ugids.size() << " unnecessary groups." << endl;);
      for (GID gid : ugids) 
        _md.mark_removed(gid);
      ++_unsat_outcomes;
      _ref_groups += ugids.size() - subset_gids.size();
      if (expecting_unsat)
        ++_unsat_correct_guesses;
    }
    // done with this check
    css.reset();
    subset_gids.clear();
    DBG(cout << "Finished the subset." << endl;);
  } // main loop
  _sat_calls = _schecker.sat_calls();
  _sat_time = _schecker.sat_time();
  if (config.get_verbosity() >= 2) {
    cout_pref << "wrkr-" << _id << " finished; "
              << " SAT calls: " << _sat_calls
              << ", SAT time: " << _sat_time << " sec" 
              << ", SAT outcomes: " << _sat_outcomes
              << " (subset singleton: " << _subset_singleton
              << ", subset non-singleton: " << _subset_nonsingleton << ")"
              << ", UNSAT outcomes: " << _unsat_outcomes
              << ", rot. points: " << _mrotter.num_points()
              << ", UNSAT guesses total(corr): " << _unsat_guesses 
              << "(" << _unsat_correct_guesses << ")"
              << endl;    
  }
}


/* Takes a number of groups in the order given by the scheduler */
void MUSExtractionAlgSubset2::make_order_subset(GIDSet& subset_gids)
{
  subset_gids.clear();
  for (GID gid; (subset_gids.size() < config.get_subset_size()) && 
         _sched.next_group(gid, _id); )
    if (_md.untested(gid))
      subset_gids.insert(gid);
}

/* Takes a group from scheduler, and builds its neighbourhood in the
 * resolution graph.
 */
void MUSExtractionAlgSubset2::make_rgraph_1hood_subset(GIDSet& subset_gids)
{
  GID gid = gid_Undef; // main clause
  subset_gids.clear();
  while(_sched.next_group(gid, _id))
    if (_md.untested(gid)) {
      subset_gids.insert(gid);
      break;
    }
  if (subset_gids.empty())      
    return;
  if (!_md.has_rgraph())
    _md.build_rgraph();
  BasicClauseVector hood;
  if (_md.gset().gclauses(gid).size() != 1) // TEMP
    throw logic_error("Resolution graph for non-singleton groups is not implemented.");
  BasicClause* cl = *_md.gset().gclauses(gid).begin();
  _md.rgraph().get_1hood(cl, hood);
  DBG(cout << "Main clause (gid= " << gid << "): " << *cl << ", 1-hood: "; 
      PRINT_PTR_ELEMENTS(hood););
  for (BasicClause* hcl : hood) {        
    if (subset_gids.size() == config.get_subset_size()) // note the case of 0 size
      break;
    if (_md.untested(hcl->get_grp_id()))
      subset_gids.insert(hcl->get_grp_id());
  }
}

// local implementations ....

namespace {

  // Returns the truth-value of clause under assignment: -1;0:+1
  int tv_clause(const IntVector& ass, const BasicClause* cl)
  {
    unsigned false_count = 0;
    for(CLiterator lpos = cl->abegin(); lpos != cl->aend(); ++lpos) {
      int var = abs(*lpos);
      if (ass[var]) {
        if ((*lpos > 0 && ass[var] == 1) ||
            (*lpos < 0 && ass[var] == -1))
          return 1;
        false_count++;
      }
    }
    return (false_count == cl->asize()) ? -1 : 0;
  }

  // Checks whether a given assignment satisfies a group; return 1 for SAT, -1
  // for UNSAT, 0 for undetermined. A set is SAT iff all clauses are SAT, a set
  // is UNSAT iff at least one clause is UNSAT, undetermined otherwise
  int tv_group(const IntVector& ass, const BasicGroupSet& gset, GID gid)
  {
    const BasicClauseVector& clauses = gset.gclauses(gid);
    unsigned sat_count = 0;
    for (cvec_citerator pcl = clauses.begin(); pcl != clauses.end(); ++pcl) {
      if ((*pcl)->removed())
        continue;
      int tv = tv_clause(ass, *pcl);
      if (tv == -1)
        return -1;
      sat_count += tv;
    }
    return (sat_count == gset.a_count(gid)) ? 1 : 0;
  }

}

/*----------------------------------------------------------------------------*/


