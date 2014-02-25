/*----------------------------------------------------------------------------*\
 * File:        mus_extraction_alg_del.cc
 *
 * Description: Implementation of the deletion-based MUS exctraction algorithm.
 *
 * Author:      antonb
 * 
 * Notes:
 *
 *                                           Copyright (c) 2011-12, Anton Belov
\*----------------------------------------------------------------------------*/

#include <cstdio>
#include <iostream>
#include <sstream>
#include "basic_group_set.hh"
#include "mus_extraction_alg.hh"

namespace {
}

/* The main extraction logic is implemented here. As usual the method does 
 * not modify the group set, but rather computes the group ids of MUS groups
 * in MUSData
 */
void MUSExtractionAlgDel::operator()(void)
{
  // now, as long as group scheduler has something to work on, do it ...
  CheckGroupStatus wi(_md, 0);    // item for SAT checks
  RotateModel rm(_md);            // item for model rotations
  GID gid = 0;
  wi.set_refine(config.get_refine_clset_mode());  // refine clset if applicable
  wi.set_need_model(config.get_model_rotate_mode());
  wi.set_use_rr(config.get_rm_red_mode() || config.get_rm_reda_mode());
  while (_sched.next_group(gid, _id)) {
    if (gid == 0) {
      if (config.get_verbosity() >= 3)
        cout_pref << "wrkr-" << _id << " skipping gid=0" << endl;
      continue;
    }
    if (config.get_verbosity() >= 3)
      cout_pref << "wrkr-" << _id << " checking gid=" << gid << " ... " << endl;
    // in the presence of refinement, model rotation etc some groups might already 
    // have been found necessary or have been removed -- check this before 
    // scheduling the work item.
    if (_md.r(gid)) {
      if (config.get_verbosity() >= 3)
        cout_pref << "wrkr-" << _id << " already removed. Skipping." << endl;
      continue;
    }
    if (_md.nec(gid)) {
      if (config.get_verbosity() >= 3)
        cout_pref << "wrkr-" << _id <<  " already known to be necessary. Skipping." << endl;
      continue;
    }

    // otherwise, do the check: set up the work item
   wi.reset();
    wi.set_gid(gid);
    _schecker.process(wi);
    if (!wi.completed()) 
      tool_abort(string("could not complete SAT check; in ")+__PRETTY_FUNCTION__);

    if (config.get_verbosity() >= 3) {
      if (wi.status())
        cout_pref << "wrkr-" << _id << " " << " group is necessary." << endl;
      else           
        cout_pref << "wrkr-" << _id << " " << wi.unnec_gids().size()           
                  << " unnecessary groups." << endl;
    }

    if (wi.status()) { // SAT
      // take care of the necessary group: put into MUSData, and mark final
      _md.mark_necessary(gid);
      _sched.update_necessary(gid);
      // do rotation, if asked for it
      if (config.get_model_rotate_mode()) {
        rm.set_gid(gid);
        rm.set_model(wi.model());
        rm.set_collect_ft_gids(config.get_reorder_mode());
        _mrotter.process(rm);
        if (rm.completed()) {
          unsigned r_count = 0;
          for (auto gid : rm.nec_gids()) {
            // double-check check if not necessary already and not gid 0
            if (gid && !_md.nec(gid)) {
              _md.mark_necessary(gid);
              _sched.update_necessary(gid);
              r_count++;
            }
          }
          if (config.get_reorder_mode()) {
            for (auto gid : rm.ft_gids()) 
              if (!_md.nec(gid))
                _sched.fasttrack(gid);
            rm.ft_gids().clear();
          }
          if ((config.get_verbosity() >= 3) && r_count)
            cout_pref << "wrkr-" << _id << " " << r_count
                      << " groups are necessary due to rotation." << endl;
          _rot_groups += r_count;
        }
        rm.reset();
      }
      _sat_outcomes++;
      if (config.get_rm_reda_mode()) // re-enable redundancy removal
        wi.set_use_rr(true);
    } else { // wi.status = UNSAT
      // take care of unnecessary groups: put each group into MUSData, and
      // mark each group as removed
      GIDSet& ugids = wi.unnec_gids();
      if (!ugids.empty()) {
        for (auto gid : ugids) {
          _md.mark_removed(gid);
          _sched.update_removed(gid);
        }       
        if (wi.tainted_core()) {
          _tainted_cores++;
          if (config.get_verbosity() >= 3)
            cout_pref << "wrkr-" << _id << " tainted core." << endl;
          // in adaptive mode, disable redundancy removal
          if (config.get_rm_reda_mode()) {
            wi.set_use_rr(false);
            if (config.get_verbosity() >= 3)
              cout_pref << "wrkr-" << _id << " temporarily disabled rr." << endl;
          }
        }
      }
      _unsat_outcomes++;
      _ref_groups += ugids.size() - 1;
    } // done with this call
  } // main loop
  _schecker.sync_solver(_md); // sync the results of the very last call
  _sat_calls = _schecker.sat_calls();
  _sat_time = _schecker.sat_time();
  if (config.get_verbosity() >= 2)
    cout_pref << "wrkr-" << _id << " finished; "
              << " SAT calls: " << _sat_calls
              << ", SAT time: " << _sat_time << " sec" 
              << ", SAT outcomes: " << _sat_outcomes
              << ", UNSAT outcomes: " << _unsat_outcomes
              << ", rot. points: " << _mrotter.num_points()
              << ", tainted cores: " << _tainted_cores
              << endl;
}

// local implementations ....

namespace {

}

/*----------------------------------------------------------------------------*/


