/*----------------------------------------------------------------------------*\
 * File:        vmus_extraction_alg_del.cc
 *
 * Description: Implementation of the deletion-based VMUS exctraction algorithm.
 *
 * Author:      antonb
 * 
 * Notes:       
 *      1. Single-threaded only for now
 *
 *                                           Copyright (c) 2011-12, Anton Belov
\*----------------------------------------------------------------------------*/

#ifdef NDEBUG
#undef NDEBUG // enable assertions (careful !)
#endif

#include <cassert>
#include <cstdio>
#include <iostream>
#include <sstream>
#include "basic_group_set.hh"
#include "check_vgroup_status.hh"
#include "mus_extraction_alg.hh"
#include "rotate_model.hh"

//#define DBG(x) x

using namespace std;
 
namespace {
}

/* The main extraction logic is implemented here. As usual the method does 
 * not modify the group set.
 */
void VMUSExtractionAlgDel::operator()(void)
{
  DBG(cout << "VMUSExtractionAlgDel::operator(), gset: " << endl;
      _md.gset().dump(););
  
  // item for SAT checks
  CheckVGroupStatus wi(_md, 0);     
  wi.set_refine(config.get_refine_clset_mode());
  wi.set_need_model(config.get_model_rotate_mode());
  wi.set_use_rr(config.get_rm_red_mode() || config.get_rm_reda_mode());

  // item for model rotations
  RotateModel rm(_md);            
  rm.set_collect_ft_gids(config.get_reorder_mode());
  rm.set_rot_depth(config.get_emr_mode() ? config.get_rotation_depth() : 0);

  // now, as long as group scheduler has something to work on, do it ...
  GID vgid = 0;
  while (_sched.next_group(vgid, _id)) {
    if (vgid == 0) {
      if (config.get_verbosity() >= 3)
        cout_pref << "wrkr-" << _id << " skipping vgid=0" << endl;
      continue;
    }
    if (config.get_verbosity() >= 3)
      cout_pref << "wrkr-" << _id << " checking vgid=" << vgid << " ... " << endl;
    // optimization: in the presence of refinement, model rotation etc some
    // groups might already have been found necessary or have been removed --
    // check this before scheduling the work item.
    bool status_known = false;
    if (_md.r(vgid)) {
      if (config.get_verbosity() >= 3)
        cout_pref << "wrkr-" << _id << " already removed. Skipping." << endl;
      status_known = true;
    } else if (_md.nec(vgid)) {
      if (config.get_verbosity() >= 3)
        cout_pref << "wrkr-" << _id <<  " already known to be necessary. Skipping." << endl;
      status_known = true;
    }
    if (status_known) { // next group
      continue;
    }
    // otherwise, do the check: set up the work item
    wi.reset();
    wi.set_vgid(vgid);

    _schecker.process(wi);

    // reset the lists for the next time !
    _md.r_list().clear();
    _md.f_list().clear();

    if (wi.completed()) {
      if (config.get_verbosity() >= 3) {
        if (wi.status())
          cout_pref << "wrkr-" << _id << " " << " group is necessary."
                       << endl;
        else
          cout_pref << "wrkr-" << _id << " " << wi.unnec_vgids().size()
                       << " unnecessary groups." << endl;
      }
      if (wi.status()) { // SAT
        if (config.get_model_rotate_mode()) {
          rm.set_gid(vgid);
          rm.set_model(wi.model());
          _mrotter.process(rm);
          if (rm.completed()) {
            unsigned r_count = 0;
            for (GIDSetIterator pvgid = rm.nec_gids().begin();
                 pvgid != rm.nec_gids().end(); ++pvgid) {
              // double-check check if not necessary already and not gid 0
              if (*pvgid && !_md.nec(*pvgid)) {
                _md.nec_gids().insert(*pvgid);
                _md.f_list().push_front(*pvgid);
                if (*pvgid != vgid)
                  r_count++;
              }
            }
            if ((config.get_verbosity() >= 3) && r_count)
              cout_pref << "wrkr-" << _id << " " << r_count
                        << " variable groups are necessary due to rotation." << endl;
            _rot_groups += r_count;
          }
          rm.reset();
        } else {
          // no model rotation -- just take care of the necessary group:        
          // put into MUSData
          _md.nec_gids().insert(vgid);
          // note that for variables can cannot always finalize the clauses --
          // the clauses can only be made final if all variables are necessary;
          // thus, the semantics of f_list() are different here
          _md.f_list().push_front(vgid);
        }
        _sat_outcomes++;
        if (config.get_rm_reda_mode()) // re-enable redundancy removal
          wi.set_use_rr(true);
      } else { // wi.status = UNSAT
        // take care of unnecessary groups: put each group into MUSData, and
        // mark each group as removed
        GIDSet& uvgids = wi.unnec_vgids();
        for (GIDSetIterator pvgid = uvgids.begin(); pvgid != uvgids.end(); ++pvgid) {
          _md.r_gids().insert(*pvgid);
          _md.r_list().push_front(*pvgid);
          // the clauses will be marked as removed (and update counts in the occlist) 
          // after they are actually removed from the solver (in SATChecker)
        }
        _unsat_outcomes++;
        if (!wi.ft_vgids().empty()) { // assume tainted core
          _tainted_cores++;
          if (config.get_verbosity() >= 3)
            cout_pref << "wrkr-" << _id << " tainted core (" << wi.ft_vgids().size()
                      << " fasttrack candidates)." << endl;
          // in adaptive mode, disable redundancy removal, and fasttrack gids
          if (config.get_rm_reda_mode()) {
            wi.set_use_rr(false);
            GIDSet& ftgids = wi.ft_vgids();
            for (GIDSetIterator pvgid = ftgids.begin(); pvgid != ftgids.end(); ++pvgid) {
              if (*pvgid != vgid)
                _sched.fasttrack(*pvgid);
            }
          }
        }
      }
      // done with this call
    } // wi.completed()
    else {
      // we will get here if for some reason the group check has not been
      // completed ... in this case we will re-schedule the check
      _sched.reschedule(vgid);
    }
  } // main loop
  _schecker.vsync_solver(_md); // sync the results of the very last call
  _sat_calls = _schecker.sat_calls();
  _sat_time = _schecker.sat_time();
  if (config.get_verbosity() >= 2)
    cout_pref << "wrkr-" << _id << " finished; "
              << " SAT calls: " << _sat_calls
              << ", SAT time: " << _sat_time << " sec" 
              << ", SAT outcomes: " << _sat_outcomes
              << ", UNSAT outcomes: " << _unsat_outcomes
              << ", tainted cores: " << _tainted_cores
              << ", rot. points: " << _mrotter.num_points()
              << endl;
  // done
}

// local implementations ....

namespace {

}

/*----------------------------------------------------------------------------*/
