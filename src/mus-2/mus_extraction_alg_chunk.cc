/*----------------------------------------------------------------------------*\
 * File:        mus_extraction_alg_chunk.cc
 *
 * Description: Implementation of the chunked deletion-based MUS exctraction 
 *              logic (ala AAAI-12).
 *
 * Author:      antonb
 * 
 * Notes:       1. MULTI_THREADED feutures are not implemented, see 
 *              MUSExtractorThreadDel if needed on how to implement them. But
 *              common code should be factored out.
 *
 *              2. This is still in a prototype shape
 *
 *                                              Copyright (c) 2012, Anton Belov
\*----------------------------------------------------------------------------*/

#ifdef NDEBUG
#undef NDEBUG // enable assertions (careful !)
#endif

#include <cassert>
#include <cstdio>
#include <iostream>
#include <sstream>
#include "basic_group_set.hh"
#include "mus_extraction_alg.hh"

using namespace std;
using namespace __gnu_cxx;

//#define DBG(x) x

namespace {

}

/* The main extraction logic is implemented here. As usual the method does 
 * not modify the group set, but rather computes the group ids of MUS groups
 * in MUSData
 */
void MUSExtractionAlgChunk::operator()(void)
{
  // +TEMP: this should extend to groups too, but not yet
  if (config.get_grp_mode())
    throw logic_error("MUSExtractionAlgChunk: group mode is not yet supported");
  // -TEMP
  BasicGroupSet& gs = _md.gset();
  unsigned chunk_size = config.get_chunk_size();
  if (chunk_size == 0)
    chunk_size = gs.gsize();
  GIDSet chunk;

  while (1) {
    // collect group IDs clauses of the next chunk
    GID gid = 0;
    chunk.clear();
    while ((chunk.size() < chunk_size) && _sched.next_group(gid, _id)) {
      if (_md.r(gid) || _md.nec(gid))
        continue;
      assert(gs.gexists(gid) && (gs.gclauses(gid).size() == 1));
      chunk.insert(gid);
    }
    if (chunk.empty()) { // all done
      DBG(cout << "No more chunks, all done." << endl;);
      break;
    }
    DBG(cout << "Got next chunk, size = " << chunk.size() << ": " << endl;
        for(GIDSet::iterator pgid = chunk.begin(); pgid != chunk.end(); ++pgid) {
          cout << "    "; (*gs.gclauses(*pgid).begin())->dump(); cout << endl;
        });
    // prepare the work items, and kick off the inner loop
    CheckGroupStatusChunk gsc(_md, gid_Undef, chunk);
    gsc.set_refine(config.get_refine_clset_mode());
    gsc.set_need_model(config.get_model_rotate_mode());
    RotateModel rm(_md);
    for (GIDSet::iterator pgid = chunk.begin(); pgid != chunk.end(); ++pgid) {
      GID gid = *pgid;
      if (config.get_verbosity() >= 3)
        cout_pref << "wrkr-" << _id << " checking gid=" << gid << " ... " << endl;
      // TEMP: in optimized version may need to move this to SATChecker
      bool status_known = false;
      _md.lock_for_reading();
      if (_md.r(gid)) {
        if (config.get_verbosity() >= 3)
          cout_pref << "wrkr-" << _id << " already removed. Skipping." << endl;
        status_known = true;
      } else if (_md.nec(gid)) {
        if (config.get_verbosity() >= 3)
          cout_pref << "wrkr-" << _id <<  " already known to be necessary. Skipping." << endl;
        status_known = true;
      }
      _md.release_lock();
      if (status_known) { // next group
        continue;
      }
      // do the check
      gsc.set_gid(gid);
      _schecker.process(gsc);
      if (!gsc.completed()) // TODO: handle this properly
        throw runtime_error("could not complete SAT check");
      if (config.get_verbosity() >= 3) {
        if (gsc.status())
          cout_pref << "wrkr-" << _id << " " << " group is necessary." << endl;
        else
          cout_pref << "wrkr-" << _id << " " << gsc.unnec_gids().size()
                    << " unnecessary groups." << endl;
      }
      if (gsc.status()) { // SAT
        // take care of the necessary group: put into MUSData, and mark final
        _md.lock_for_writing();
        _md.nec_gids().insert(gid);
        _md.f_list().push_front(gid);
        _md.release_lock();
        // do rotation, if asked for it
        if (config.get_model_rotate_mode()) {
          rm.set_gid(gid);
          rm.set_model(gsc.model()); // this is safe b/c the same thread that did
          // the SAT check is doing rotation
          rm.set_rot_depth(config.get_rotation_depth());
          rm.set_rot_width(config.get_rotation_width());
          rm.set_ignore_g0(config.get_ig0_mode());
          rm.set_ignore_global(config.get_iglob_mode());
          _mrotter.process(rm);
          if (rm.completed()) {
            unsigned r_count = 0;
            _md.lock_for_writing();
            for (GIDSetIterator pgid = rm.nec_gids().begin();
                 pgid != rm.nec_gids().end(); ++pgid) {
              // double-check check if not necessary already and not gid 0
              if (*pgid && !_md.nec(*pgid)) {
                _md.nec_gids().insert(*pgid);
                _md.f_list().push_front(*pgid);
                r_count++;
              }
            }
            _md.release_lock();
            if ((config.get_verbosity() >= 3) && r_count)
              cout_pref << "wrkr-" << _id << " " << r_count
                        << " groups are necessary due to rotation." << endl;
            _rot_groups += r_count;
          }
          rm.reset();
        }
        ++_sat_outcomes;
      } else { // gsc.status = UNSAT
        // take care of unnecessary groups
        GIDSet& ugids = gsc.unnec_gids();
        if (!ugids.empty()) {
          _md.lock_for_writing();
          if (gsc.version() == _md.version()) {
            for (GIDSetIterator pgid = ugids.begin(); pgid != ugids.end(); ++pgid) {
              _md.r_gids().insert(*pgid);
              _md.r_list().push_front(*pgid);
              // mark the clauses as removed (and update counts in the occlist)
              BasicClauseVector& clv = gs.gclauses(*pgid);
              for (cvec_iterator pcl = clv.begin(); pcl != clv.end(); ++pcl) {
                if (!(*pcl)->removed()) {
                  (*pcl)->mark_removed();
                  if (gs.has_occs_list())
                    gs.occs_list().update_active_sizes(*pcl);
                }
              }
            }
            // removed some clauses -- increment the version
            _md.incr_version();
          } else {
            // version mismatch -- re-try the same group
            --pgid;
          }
          _md.release_lock();
        }
        ++_unsat_outcomes;
        _ref_groups += ugids.size() - 1;
      }
      // done with this check
      gsc.reset();  // (this will set first to false)
    } // inner loop
    DBG(cout << "Finished the chunk." << endl;);
  } // main loop
  _sat_calls = _schecker.sat_calls();
  _sat_time = _schecker.sat_time();
  if (config.get_verbosity() >= 2)
    cout_pref << "wrkr-" << _id << " finished; "
              << " SAT calls: " << _sat_calls
              << ", SAT time: " << _sat_time << " sec" 
              << ", SAT outcomes: " << _sat_outcomes
              << ", UNSAT outcomes: " << _unsat_outcomes
              << ", rot. points: " << _mrotter.num_points()
              << endl;
}

// local implementations ....

namespace {

}

/*----------------------------------------------------------------------------*/


