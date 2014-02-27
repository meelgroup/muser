/*----------------------------------------------------------------------------*\
 * File:        mus_extraction_alg_ins.cc
 *
 * Description: Implementation of the insertion-based MUS extraction logic.
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
void MUSExtractionAlgIns::operator()(void)
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
    crs.set_allend(p_removed);
    auto p_curr = p_unknown;
    while (p_curr <= p_removed) {
      // test [0,p_curr) for SAT; if SAT - continue, o/w - got new necessary 
      // group (or already have an MUS if p_curr == p_unknown)
      crs.set_begin(p_unknown);
      crs.set_end(p_curr);
      DBG(cout << "  inner loop: testing "; 
          copy(p_unknown, p_curr, ostream_iterator<GID>(cout, " ")); cout << endl;);
      _schecker.process(crs);
      _md.clear_lists(); 
      if (!crs.completed()) // TODO: handle this properly
        tool_abort(string("could not complete SAT check; in ")+__PRETTY_FUNCTION__);
      if (crs.status()) { // SAT
        DBG(cout << "  status: SAT" << endl;);
        // make a copy of the model -- we may need it for later
        if (config.get_model_rotate_mode())
          last_model = crs.model();
        _sat_outcomes++;
      } else { // UNSAT -- found new transition group, or already have an MUS
        DBG(cout << "  status: UNSAT ";
            if (p_curr == p_unknown) cout << "(got the MUS)" << endl;
            else cout << "(new transition group " << *(p_curr-1) << ")" << endl;);
        _unsat_outcomes++;
        break;
      }
      ++p_curr;
    }
    // if this loop is ended in SAT, then if we're in MUS mode,  we had a SAT 
    // instance, otherwise something when really wrong
    if (crs.status()) {
      if (config.get_mus_mode()) 
        tool_abort(string("satisfiable instance is given to an insertion-mode"
                          "MUS extractor; in ")+__PRETTY_FUNCTION__);
      else
        tool_abort(string("inner loop ended in SAT, something is wrong; in ")+
                   __PRETTY_FUNCTION__);
    }

    // now, p_curr-1 points to the new necessary group, so:
    //  1. mark [p_curr,p_removed) as removed and set p_removed to p_curr
    //    1.1 if refinement is on, shift all unnecessary groups so that 
    //        [p_curr,p_removed) still contains the groups to be removed
    //  2. if p_removed == p_unknown now, then we're done (got the MUS),
    //    otherwise mark p_curr-1 as necessary
    //    2.1 see if any others group are necessary because of RMR, if so
    //        goto 2.
    //  3. put all necessary groups to position from p_unknown, and move
    //     p_unknown to the end of the necessary groups

    if (config.get_verbosity() >= 2)
      cout_pref << "wrkr-" << _id << " found new necessary group using "
                << (_schecker.sat_calls() - _sat_calls) << " SAT calls, "
                << (_schecker.sat_time() - _sat_time) << " sec SAT time."
                << endl;

    // unnecessary groups (refinement is only for MUS mode)
    if (config.get_mus_mode() && config.get_refine_clset_mode()) {
      const GIDSet& unnec_gids = crs.unnec_gids();
      DBG(cout << "  refinement: " << unnec_gids.size() << " unnecessary GIDs " 
          << unnec_gids << endl;);
      assert(!unnec_gids.count(*p_curr)); // cannot be unnecessary !
      // move all unnecessary groups to the end
      auto p = stable_partition(p_unknown, p_curr, 
                                [&unnec_gids](GID gid) { return !unnec_gids.count(gid); });
      assert((p_curr - p) == (int)unnec_gids.size());
      p_curr = p;
      _ref_groups += unnec_gids.size();
    }
    for_each(p_curr, p_removed, [&](GID gid) { _md.mark_removed(gid); });
    if (config.get_verbosity() >= 2)
      cout_pref << "wrkr-" << _id << " " << (p_removed - p_curr)
                << " unnecessary groups." << endl;
    p_removed = p_curr;
    
    // necessary groups (up to p_curr) -- but only if don't have the MUS yet
    if (p_unknown < p_removed) {
      if (config.get_model_rotate_mode() && !last_model.empty()) {
        // note that whatever model rotation finds us must be in the region [0,p_curr)
        rm.set_gid(*(p_curr-1));
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
      nec_gids.insert(*(p_curr-1));   // tag along *p_curr, and process all
      DBG(cout << "  " << nec_gids.size() << " necessary groups: " << nec_gids << endl;);
      // move all necessary groups to the front
      auto p = stable_partition(p_unknown, p_curr, 
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


