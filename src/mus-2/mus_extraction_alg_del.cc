/*----------------------------------------------------------------------------*\
 * File:        mus_extraction_alg_del.cc
 *
 * Description: Implementation of the deletion-based MUS exctraction algorithm.
 *              The logic is packaged as a class that can be used to run on 
 *              multiple threads. Make sure to define MULTI_THREADED to enable 
 *              features required to run on multiple threads.
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
#ifdef MULTI_THREADED
#if __linux__
#include <sched.h>
#endif
#include <tbb/tbb.h>
#include <tbb/tbb_thread.h>
//#include <tbb/compat/thread>
#endif
#include "basic_group_set.hh"
#include "mus_extraction_alg.hh"

namespace {

#ifdef MULTI_THREADED
  /** Provides "one-shot" output which is better for mutlithreaded logging and
   *  debugging. Note that there's an overhead and an explicit serialization of
   *  access to stdout here, so use judiciously
   */
  class _out_mt {
  public:
    _out_mt(const char* pref = "") : _pref(pref) {}
    ~_out_mt(void) {
      _out.flush();
      printf("%s%s", _pref, _out.str().c_str());
      fflush(stdout);
    }
    std::ostringstream& get(void) {
      _out << "tid 0x" << hex << tbb::this_tbb_thread::get_id()
           << ": " << dec;
      return _out;
    }
  private:
    std::ostringstream _out;
    const char* _pref;
  };

  /** Sets the hard affinity of the calling thread to the CPU with the specified
   * ID. The id's on linux are in the range [0,num_logical_cpu's). Nooped on Mac
   */
  void set_hard_affinity(unsigned id);
#endif // MULTI_THREADED

#if STATS  
  /** Timing stats, per group */
  class TimingStats {
    struct group_stats {
      double time;        // spent on this group
      bool status;        // sat/unsat
      unsigned id;        // which worker
      unsigned tries;     // how many tries
      group_stats() : time(0),  status(false), id(0), tries(0) {}
    };
    // map gid to group_stats
    typedef map<GID,group_stats> stat_map;
    stat_map _gmap;
  public:
    // NOTE: assumes that no 2 threads will be reporting the same group at the
    // same time
    void report_stats(GID gid, double time, bool status, unsigned id)
    {
      group_stats& gs = _gmap[gid];
      gs.time += time;
      gs.status = status;
      gs.id = id;
      gs.tries++;
    }
    // This will print in format suitable for directly feeding into gnuplot
    void print_stats(void) {
      cout << "# gid, time, status, id, tries" << endl;
      for (stat_map::iterator ps = _gmap.begin(); ps != _gmap.end(); ++ps) {
        group_stats& gs = ps->second;
        cout << ps->first << " "
             << gs.time << " "
             << gs.status << " "
             << gs.id << " "
             << gs.tries << endl;
      }
    }
  };
  TimingStats ts; // one instance
#endif  // STATS

}

// output macros ...
#ifndef MULTI_THREADED
#define cout_pref_mt cout_pref
#define cout_mt cout
#else
#define cout_pref_mt (_out_mt(config.get_prefix()).get())
#define cout_mt (_out_mt().get())
#endif


/* The main extraction logic is implemented here. As usual the method does 
 * not modify the group set, but rather computes the group ids of MUS groups
 * in MUSData
 */
void MUSExtractionAlgDel::operator()(void)
{
#ifdef MULTI_THREADED      
  set_hard_affinity(_id);     // use thread ID as CPU ID
#endif
  // now, as long as group scheduler has something to work on, do it ...
  // if retry_last_gid = true we assume that a last group gid needs to be
  // re-checked
  CheckGroupStatus wi(_md, 0);    // item for SAT checks
  RotateModel rm(_md);            // item for model rotations
  GID gid = 0;
  bool retry_last_gid = false;
  double start_cpu_time = RUSAGE::read_cpu_time();
  unsigned n_iter = 0;
  wi.set_refine(config.get_refine_clset_mode());  // refine clset if applicable
  wi.set_need_model(config.get_model_rotate_mode());
  wi.set_use_rr(config.get_rm_red_mode() || config.get_rm_reda_mode() 
                || config.get_irr_mode());
  if (config.get_approx_mode()) {
    wi.set_conf_limit(config.get_approx_conf_lim());
    wi.set_cpu_limit(config.get_approx_cpu_lim());
  }
  while (retry_last_gid || _sched.next_group(gid, _id)) {
    retry_last_gid = false; // will be set to true when needed
    if (gid == 0) {
      if (config.get_verbosity() >= 4)
        cout_pref_mt << "wrkr-" << _id << " skipping gid=0" << endl;
      continue;
    }
    if (config.get_verbosity() >= 4)
      cout_pref_mt << "wrkr-" << _id << " checking gid=" << gid << " ... " << endl;
    // optimization: in the presence of refinement, model rotation etc some
    // groups might already have been found necessary or have been removed --
    // check this before scheduling the work item. Note that in multithreaded
    // environment this check does not guarantee that by the time the work
    // item is executed the status of the group is still unknown -- this is ok
    // though, because the workers will check this anyway before running the
    // SAT check
    bool status_known = false;
    _md.lock_for_reading();
    if (_md.r(gid)) {
      if (config.get_verbosity() >= 4)
        cout_pref_mt << "wrkr-" << _id << " already removed. Skipping." << endl;
      status_known = true;
    } else if (_md.nec(gid)) {
      if (config.get_verbosity() >= 4)
        cout_pref_mt << "wrkr-" << _id <<  " already known to be necessary. Skipping." << endl;
      status_known = true;
    }
    _md.release_lock();
    if (status_known) { // next group
      continue;
    }
    // otherwise, do the check: set up the work item
    wi.reset();
    wi.set_gid(gid);
#if (__linux__ && FULLDEBUG) || (__linux__ && STATS)
    double curr_t = _schecker.sat_time();
#endif
    _schecker.process(wi);
#if (__linux__ && STATS)
    ts.report_stats(gid, (_schecker.sat_time()-curr_t), wi.status(), _id);
#endif
#if (__linux__ && FULLDEBUG)
    cout_mt << "wrkr-" << _id << " "
            << (wi.completed() ? "done" : "NOT done")
            << (wi.completed() ? (wi.status() ? "(SAT)" : "(UNSAT)") : "")
            << " in "
            << (_schecker.sat_time()-curr_t) << " sec" << endl;
#endif
    if (wi.completed()) {
      if (config.get_verbosity() >= 4) {
        if (wi.status())
          cout_pref_mt << "wrkr-" << _id << " " << " group is necessary."
                       << endl;
        else
          cout_pref_mt << "wrkr-" << _id << " " << wi.unnec_gids().size()
                       << " unnecessary groups." << endl;
      }
      if (wi.status()) { // SAT
        // take care of the necessary group: put into MUSData, and mark final
        _md.lock_for_writing();
        _md.mark_necessary(gid);
        _md.release_lock();
        _sched.update_necessary(gid);
        // do rotation, if asked for it (and there're untested groups)
        if (config.get_model_rotate_mode() && _md.num_untested()) {
          rm.set_gid(gid);
          rm.set_model(wi.model()); // this is safe b/c the same thread that did
                                    // the SAT check is doing rotation
          rm.set_rot_depth(config.get_rotation_depth());
          rm.set_rot_width(config.get_rotation_width());
          rm.set_collect_ft_gids(config.get_reorder_mode());
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
                _md.mark_necessary(*pgid);
                _sched.update_necessary(*pgid);
                r_count++;
              }
            }
            if (config.get_reorder_mode()) {
              for (GIDSet::iterator pr = rm.ft_gids().begin(); 
                   pr != rm.ft_gids().end(); ++pr)
                if (!_md.nec(*pr))
                  _sched.fasttrack(*pr);
              rm.ft_gids().clear();
            }

            _md.release_lock();
            if ((config.get_verbosity() >= 4) && r_count)
              cout_pref_mt << "wrkr-" << _id << " " << r_count
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
        // mark each group as removed; for the sequential processing there is
        // no need to check the version number - however, for parallel version
        // we must look at the version number inside wi.version() -- if the
        // current version of MUSData is ahead of wi.version() this means that
        // we have removed some groups while the item was being processed, in
        // which case the unnecessary gids may not be unnecessary anymore. The
        // simplest way to handle this is by simply discaring the result: this
        // code is below
        GIDSet& ugids = wi.unnec_gids();
        if (!ugids.empty()) {
          _md.lock_for_writing();
          if (wi.version() == _md.version()) {
            for (GIDSetIterator pgid = ugids.begin(); pgid != ugids.end(); ++pgid) {
              _md.mark_removed(*pgid);
              _sched.update_removed(*pgid);
            }
            // removed some clauses -- increment the version
            _md.incr_version();
            if (wi.tainted_core()) {
              _tainted_cores++;
              if (config.get_verbosity() >= 4)
                cout_pref << "wrkr-" << _id << " tainted core." << endl;
              // in adaptive mode, disable redundancy removal
              if (config.get_rm_reda_mode()) {
                wi.set_use_rr(false);
                if (config.get_verbosity() >= 4)
                  cout_pref << "wrkr-" << _id << " temporarily disabled rr." << endl;
              }
            }
          } else {
            // version mismatch -- re-try the same group
            retry_last_gid = true;
          }
          _md.release_lock();
        }
        _unsat_outcomes++;
        _ref_groups += ugids.size() - 1;
      }
      // done with this call
    } // wi.completed()
    else {
      if (config.get_verbosity() >= 4)
        cout_pref_mt << "wrkr-" << _id << " " << " group status is unknown." << endl;
      ++_unknown_outcomes;
      switch(config.get_approx_mode()) {
      case 1: // overaproximation - treat as necessary
        _md.lock_for_writing();
        _md.mark_necessary(gid, true);
        _md.release_lock();
        _sched.update_necessary(gid);
        if (config.get_verbosity() >= 4)
          cout_pref_mt << "wrkr-" << _id << " " << " treating as neccessary." << endl;
        break;
      case 2: // underappoximation - treat as unnecessary
        _md.lock_for_writing();
        _md.mark_removed(gid, true);
        _md.incr_version();
        _md.release_lock();
        _sched.update_removed(gid);
        if (config.get_verbosity() >= 4)
          cout_pref_mt << "wrkr-" << _id << " " << " treating as unneccessary." << endl;
        break;
      case 3: // resheduing
        // TODO: increase limits, if its time to do it
        tool_abort("approximation mode 3 is not yet implemented");
      default:
        _sched.reschedule(gid);
      }
    }
    // check the cpu limit
    if (_cpu_time_limit && (RUSAGE::read_cpu_time() - start_cpu_time >= _cpu_time_limit)) {
      if (config.get_verbosity() >= 3)
        cout_pref_mt << "wrkr-" << _id << " reached CPU time limit." << endl;
      break;
    }
    // check iteration limit
    if (_iter_limit && ++n_iter >= _iter_limit) {
      if (config.get_verbosity() >= 3)
        cout_pref_mt << "wrkr-" << _id << " reached iteration limit." << endl;
      break;
    }
    if (config.get_verbosity() >= 3)
      cout_pref_mt << "[" << RUSAGE::read_cpu_time() << " sec] "
                   << "wrkr-" << _id << ": nec = " << _md.nec_gids().size()
                   << ", unn = " << _md.r_gids().size() 
                   << ", unk = " << _md.num_untested()
                   << (config.get_approx_mode() ? (", fake = "+convert<int>(_md.num_fake())) : "")
                    << endl;
  } // main loop
  _schecker.sync_solver(_md); // sync the results of the very last call
  _sat_calls = _schecker.sat_calls();
  _sat_time = _schecker.sat_time();
  if (config.get_verbosity() >= 2) {
    cout_pref_mt << "wrkr-" << _id << " finished; "
                 << " SAT calls: " << _sat_calls
                 << ", SAT time: " << _sat_time << " sec" 
                 << ", SAT outcomes: " << _sat_outcomes
                 << ", UNSAT outcomes: " << _unsat_outcomes
                 << ", rot. points: " << _mrotter.num_points()
                 << ", tainted cores: " << _tainted_cores
                 << ", SAT time SAT: " << _schecker.sat_time_sat() 
                 << " sec (" << _schecker.sat_time_sat()/_sat_outcomes << " sec/call)"
                 << ", SAT time UNSAT: " << _schecker.sat_time_unsat() 
                 << " sec (" << _schecker.sat_time_unsat()/_unsat_outcomes << " sec/call)"
                 << (config.get_approx_mode() ? (", UNKNOWN outcomes = "+convert<int>(_unknown_outcomes)) : "")
                 << endl;
    _mrotter.print_stats();     // TODO: pass the predix
  }
#if STATS
  ts.print_stats();
#endif
}

// local implementations ....

namespace {

#ifdef MULTI_THREADED
  /** Sets the hard affinity of the calling thread to the CPU with the specified
   * ID. The id's on linux are in the range [0,num_logical_cpu's). Nooped on Mac
   */
  void set_hard_affinity(unsigned id)
  {
#if __linux__
    DBG(cout_mt << "setting affinity to CPU " << id << endl;);
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(id, &mask);
    int res = sched_setaffinity(0, sizeof(mask), &mask);
    DBG(if (res == -1) cout_mt << "failed to set affinity" << endl;);
#endif
#if (__APPLE__ & __MACH__)
    DBG(cout_mt << "warning: cannot set affinity to CPU " << id << " on Mac" << endl;);
#endif
  }
#endif

}

/*----------------------------------------------------------------------------*/


