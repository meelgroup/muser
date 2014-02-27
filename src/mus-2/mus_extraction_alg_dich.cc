/*----------------------------------------------------------------------------*\
 * File:        mus_extraction_alg_dich.cc
 *
 * Description: Implementation of the dichotomic MUS extraction logic.
 *
 * Author:      antonb
 * 
 * Notes:       1. MULTI_THREADED features are removed completely; clears the
 *              lists inside MUSData. 
 *
 *
 *                                              Copyright (c) 2012, Anton Belov
\*----------------------------------------------------------------------------*/

#include <cassert>
#include <cstdio>
#include <iostream>
#include <sstream>
#include "basic_group_set.hh"
#include "check_range_status.hh"
#include "mus_extraction_alg.hh"

using namespace std;
//using namespace __gnu_cxx;

//#define DBG(x) x

namespace {

}

/* The main extraction logic is implemented here. As usual the method does 
 * not modify the group set, but rather computes the group ids of MUS groups
 * in MUSData
 */
void MUSExtractionAlgDich::operator()(void)
{
  // this is the vector with all GIDs, as given by the scheduler; the indexes
  // in the vector will separate the necessary clauses from untested and from
  // removed clauses
  vector<GID> all_gids;
  for (GID gid; _sched.next_group(gid, _id); all_gids.push_back(gid));
  auto p_unknown = all_gids.begin(); // start of unknown; everything before is MUS
  auto p_removed = all_gids.end(); // start of removed; everything after is gone

  // work items
  CheckRangeStatus crs(_md);
  crs.set_refine(config.get_mus_mode() && config.get_refine_clset_mode());
  crs.set_need_model(config.get_model_rotate_mode());
  crs.set_add_negation(config.get_irr_mode());
  RotateModel rm(_md);

  // main loop
  while (p_unknown != p_removed) { 
    DBG(cout << "Main loop: " << (p_removed - p_unknown) << 
        " groups in the working set:";
        cout << endl << "  necessary (" << (p_unknown-all_gids.begin()) << "): ";
        copy(all_gids.begin(), p_unknown, ostream_iterator<GID>(cout, " ")); 
        cout << endl << "  unknown (" << (p_removed-p_unknown) << "): ";
        copy(p_unknown, p_removed, ostream_iterator<GID>(cout, " ")); 
        cout << endl << "  removed (" << (all_gids.end()-p_removed) << "): ";
        copy(p_removed, all_gids.end(), ostream_iterator<GID>(cout, " ")); 
        cout << endl;);
    // inner loop -- from p_unknown to p_removed
    IntVector last_model;       // copy of the most recent model (for rotation)
    auto p_min = p_unknown;
    auto p_max = p_removed;
    auto p_mid = p_unknown;
    do {
      // test [0,p_mid) for SAT; adjust p_min or p_max according to the outcome
      crs.reset();
      crs.set_begin(p_unknown);
      crs.set_end(p_mid);
      crs.set_allend(p_removed);
      DBG(cout << "  inner loop: testing "; 
          copy(p_unknown, p_mid, ostream_iterator<GID>(cout, " ")); cout << endl;);
      _schecker.process(crs);
      _md.clear_lists(); 
      if (!crs.completed()) // TODO: handle this properly
        tool_abort(string("could not complete SAT check; in ")+__PRETTY_FUNCTION__);
      if (crs.status()) { // SAT
        DBG(cout << "  status: SAT" << endl;);
        p_min = p_mid;
        // make a copy of the model -- we may need it for later
        if (config.get_model_rotate_mode())
          last_model = crs.model();
        _sat_outcomes++;
      } else { // UNSAT refine and drop clauses
        DBG(cout << "  status: UNSAT";
            if (p_mid == p_unknown) cout << "(got the MUS)"; cout << endl;);
        // beside the groups in [p_mid, p_removed) we may have unnecessary
        // groups within [p_unknown, p_mid) if refinement is ok -- get them and 
        // shift them to the end
        if (config.get_mus_mode() && config.get_refine_clset_mode()) {
          const GIDSet& unnec_gids = crs.unnec_gids();
          DBG(cout << "  refinement: " << unnec_gids.size() << " unnecessary GIDs " 
              << unnec_gids << endl;);
          // move all unnecessary groups to the end; but note that every 
          // unneccessary group before p_min will be removed, and so p_min needs 
          // to be moved left the appropriate number of positions
          int num_rem = count_if(p_unknown, p_min, [&](GID gid) 
                                 { return unnec_gids.count(gid); });
          //cout << "moving p_min by " << num_rem << " positions" << endl;
          p_min = max(p_min - num_rem, p_unknown);
          auto p = stable_partition(p_unknown, p_mid, 
                                    [&](GID gid) { return !unnec_gids.count(gid); });
          assert((p_mid - p) == (int)unnec_gids.size());
          p_mid = p;
          _ref_groups += unnec_gids.size();
        }
        for_each(p_mid, p_removed, [&](GID gid) { _md.mark_removed(gid); });
        if (config.get_verbosity() >= 2)
          cout_pref << "wrkr-" << _id << " " << (p_removed - p_mid)
                    << " unnecessary groups." << endl;
        p_removed = p_mid;
        p_max = p_mid;
        _unsat_outcomes++;
      }
      p_mid = p_min + (p_max - p_min)/2;
      DBG(cout << "Current pointers: p_min = " << (p_min - p_unknown) 
          << ", p_mid = "  << (p_mid - p_unknown) 
          << ", p_max = " << (p_max - p_unknown) << endl;);
    } while (p_min < p_max - 1);
    // we're here if p_min = p_max - 1 or if p_min = p_max; in the former case
    // we have a new transition group in p_min, in the latter we already have 
    // an MUS in [0, p_min); note that all unnecessary groups have already
    // been removed, so we only need to take care of necessary groups

    assert((p_min == p_max - 1) || ((p_min == p_max) && (p_min == p_unknown)));

    // if we haven't got the MUS yet, p_min points to the new necessary group; 
    // other1wise we already have p_removed = p_unknown, because p_min == p_max
    // and p_min == p_unknown
    if (p_min == p_max - 1) {    
      if (config.get_verbosity() >= 2)
        cout_pref << "wrkr-" << _id << " found new necessary group using "
                  << (_schecker.sat_calls() - _sat_calls) << " SAT calls, "
                  << (_schecker.sat_time() - _sat_time) << " sec SAT time."
                  << endl;
      // necessary groups (up to p_min) from model rotation
      if (config.get_model_rotate_mode() && !last_model.empty()) {
        // note that whatever model rotation finds us must be in the region [0,p_curr)
        rm.set_gid(*p_min);
        rm.set_model(last_model);
        rm.set_rot_depth(config.get_rotation_depth());
        rm.set_rot_width(config.get_rotation_width());
        rm.set_ignore_g0(config.get_ig0_mode());
        rm.set_ignore_global(config.get_iglob_mode());
        _mrotter.process(rm);
        if (!rm.completed())
          tool_abort(string("could not complete model rotation; in ")+__PRETTY_FUNCTION__);
      }
      GIDSet& nec_gids = rm.nec_gids();
      nec_gids.insert(*p_min);   // tag along *p_min, and process all
      DBG(cout << "  " << nec_gids.size() << " necessary groups: " << nec_gids << endl;);
      // move all necessary groups to the front
      auto p = stable_partition(p_unknown, p_removed, 
                                [&nec_gids](GID gid) { return nec_gids.count(gid); });
      assert((p - p_unknown) == (int)nec_gids.size());
      for_each(p_unknown, p, [&](GID gid) { _md.mark_necessary(gid); });
      if (config.get_verbosity() >= 2)
        cout_pref << "wrkr-" << _id << " " << (p - p_unknown)
                  << " necessary groups." << endl;
      p_unknown = p;
      _rot_groups += nec_gids.size() - 1;
      rm.reset();
    }
    crs.reset();
    _sat_calls = _schecker.sat_calls();
    _sat_time = _schecker.sat_time();
  } // main loop
  if (config.get_verbosity() >= 2)
    cout_pref << "wrkr-" << _id << " finished; "
              << " SAT calls: " << _sat_calls
              << ", SAT time: " << _sat_time << " sec" 
              << ", SAT outcomes: " << _sat_outcomes
              << ", UNSAT outcomes: " << _unsat_outcomes
              << ", ref. groups: " << _ref_groups
              << ", rot. groups: " << _rot_groups
              << ", rot. points: " << _mrotter.num_points()
              << endl;
}

// local implementations ....

namespace {

}

/*----------------------------------------------------------------------------*/


