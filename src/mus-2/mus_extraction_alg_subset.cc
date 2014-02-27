/*----------------------------------------------------------------------------*\
 * File:        mus_extraction_alg_subset.cc
 *
 * Description: Implementation of the subset-based deletion-based MUS 
 *              exctraction algorithm (ala ECAI-12 submission).
 *
 * Author:      antonb
 * 
 * Notes:       1. MULTI_THREADED feutures are not implemented, see 
 *              MUSExtractorThreadDel if needed on how to implement them. But
 *              common code should be factored out.
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
#include <queue>
#include "basic_group_set.hh"
#include "id_manager.hh"
#include "mus_extraction_alg.hh"
//#include "picosat_mi.hh"
#include "solver_wrapper.hh"
#include "solver_factory.hh"
#include "trace_analyzer.hh"

using namespace std;
using namespace __gnu_cxx;

//#define DBG(x) x
//#define __linux__ 0

namespace {

  // Returns the truth-value of clause under assignment: -1;0:+1
  int tv_clause(const IntVector& ass, const BasicClause* cl);
  // Checks whether a given assignment satisfies a given group: -1;0;+1
  int tv_group(const IntVector& ass, const BasicGroupSet& gset, GID gid);

  /** A wrapper to abstract the FILE* away -- the imlementation differs on 
   * Linux (memory streams) and Mac (file stream)
   */
  class FILEStream {

  public:       

    /* Constructor initializes the stream */
    FILEStream(void) {
#if __linux__
      _fstream = open_memstream(&_buffer, &_size);
      NDBG(cout << "Initial: " << hex << (void*)_buffer << dec << ", size: " << _size << endl;);
#else 
      srandom(getpid());
      sprintf(_tfile_name, "/tmp/prooftrace-%d-%ld.txt", getpid(), random());
      _fstream = fopen(_tfile_name, "w+");
#endif
      if (!_fstream)
        throw runtime_error("MUSExtractionAlgSubset: unable to create proof stream.");
    }
    /* Destructor closes everything down */
    ~FILEStream(void) {
      fclose(_fstream);
#if __linux__
      free(_buffer);
#else 
      unlink(_tfile_name);
#endif
    }
    /* Returns the underlying stream */
    FILE* fstream(void) { return _fstream; }
    /* Rewinds the stream -- the content is kept */
    void rewind(void) { fseek(_fstream, 0, SEEK_SET); }
    /* Resets the stream -- i.e. clears; call fstream() after this as the handle
     * might change */
    void reset(void) {
#if __linux__
      fflush(_fstream);
      fseek(_fstream, 0, SEEK_SET);
      //memset(_buffer, 0, _size);
      //fclose(_fstream);
      // free(_buffer);
      //_fstream = open_memstream(&_buffer, &_size);
      NDBG(cout << "Reset: " << hex << (void*)_buffer << dec << ", size: " << _size << endl;);
#else
      fclose(_fstream);
      _fstream = fopen(_tfile_name, "w+");
#endif
      if (!_fstream)
        throw runtime_error("MUSExtractionAlgSubset: unable to create proof stream.");
    }
    /* Flushes the stream */
    void flush(void) { fflush(_fstream); }

  private:

    FILE* _fstream;        // the stream itself
#if __linux__
    char* _buffer;         // pointer to the memory buffer
    size_t _size;          // size of the buffer
#else
    char _tfile_name[255]; // temporary file name
#endif

  };

  /** Helper class to sort groups accoring to the path counts 
   */
  class PCOrder {
    const TraceAnalyzer::PathCountMap& _pm;
    const BasicGroupSet& _gs;           // reference to the groupset
    unsigned _order;                    // order
  public:       
    // order = 1 means smallest first; order = 2 means largest count first
    PCOrder(const TraceAnalyzer::PathCountMap& pm, 
            const BasicGroupSet& gs, 
            unsigned order) : _pm(pm), _gs(gs), _order(order) {}
    // comparator: operator() returns true if g1 < g2; since priority_queue 
    // gives the greatest element first, we will return true if the sum of 
    // path counts of g2 is smaller than that of g1 (for order = 1)
    bool operator()(GID g1, GID g2) {
      const BasicClauseVector& cls1 = _gs.gclauses(g1);
      double sum1 = 0;
      for (BasicClauseVector::const_iterator pcl = cls1.begin(); 
           pcl != cls1.end(); ++pcl)
        if (!(*pcl)->removed()) {
          TraceAnalyzer::PathCountMap::const_iterator pm = _pm.find((*pcl)->ss_id()+1);
          sum1 += (pm == _pm.end()) ? 0 : pm->second;
        }
      const BasicClauseVector& cls2 = _gs.gclauses(g2);
      double sum2 = 0;
      for (BasicClauseVector::const_iterator pcl = cls2.begin(); 
           pcl != cls2.end(); ++pcl)
        if (!(*pcl)->removed()) {
          TraceAnalyzer::PathCountMap::const_iterator pm = _pm.find((*pcl)->ss_id()+1);
          sum2 += (pm == _pm.end()) ? 0 : pm->second;
        }
      NDBG(cout << "gid1=" << g1 << ", path count=" << sum1 
           << " gid2=" << g2 << ", path count=" << sum2 << endl;);
      return ((_order == 1) ? (sum2 < sum1) : (sum2 > sum1));
    }
  };
  // priority queue using the comparator above
  typedef std::priority_queue<GID, vector<GID>, PCOrder> PCQueue;;

}

    
/* The main extraction logic is implemented here. As usual the method does 
 * not modify the group set, but rather computes the group ids of MUS groups
 * in MUSData;
 * TODO: the algorithms with and without heuristics are quite a bit different
 * maybe its worthwhile to re-factor
 */
void MUSExtractionAlgSubset::operator()(void)
{
  // +TEMP: this should extend to groups too, but not yet
  if (config.get_grp_mode())
    throw logic_error("MUSExtractionAlgSubset: group mode is not yet supported");
  // -TEMP

  BasicGroupSet& gset = _md.gset();
  GIDSet subset_gids;  // the "subset"
  CheckSubsetStatus css(_md, subset_gids);
  css.set_refine(config.get_refine_clset_mode());
  css.set_need_model(true);
  css.set_use_rr(false); // TEMP
  RotateModel rm(_md); // item for model rotations
  GIDSet single_mode_gids; // groups to be tested in single-group mode 
                           // (in case of non-singleton SAT)
  bool has_trace = false;  // when true, we have a trace to work with ...
  bool turned_off = false; // when true, subsetting has been turned off
  PCQueue* pcq = 0;        // priority queue (by path count)

  // trace analyzer setup
  TraceAnalyzer ta;
  FILEStream* tfile = new FILEStream(); // for proper comparison, use always; 
                                        // otherwise (config.get_subset_mode() > 0) ? new FILEStream() : 0;

  // main loop
  while (1) {
    bool expecting_unsat = false; // for stats // expecting_sat = false,
    // populate the current subset
    assert(subset_gids.empty());
    // if single_mode_gids is not empty, then we are in a single-group mode -- 
    // take the first one and make a singleton subset; otherwise populate the 
    // subset based on various heuristics (to be refactored)
    while (!single_mode_gids.empty()) {
      GIDSet::iterator pb = single_mode_gids.begin();
      GID gid = *pb;
      single_mode_gids.erase(pb);
      if (_md.untested(gid)) {
        subset_gids.insert(gid);
        break;
      }
    }
    DBG(if (!subset_gids.empty())
          cout << "Working in the single-group mode, testing gid = " << *subset_gids.begin() << endl;);
    // if subset_gids is empty now, then need to make the next subset
    if (subset_gids.empty()) {
      //
      // This is where we need to calculate the subset ... we might use 
      // different heustics, but only if we already have trace
      // 
      if (turned_off) { 
        // subsetting got turned off, just take one next group from the scheduler
        for (GID gid; _sched.next_group(gid, _id); )
          if (_md.untested(gid)) {
            subset_gids.insert(gid);
            break;
          }
      } else if ((config.get_subset_mode() == 0) || !has_trace) {
        // no heuristics -- just take some number of groups from the scheduler
        for (GID gid; (subset_gids.size() < config.get_subset_size()) && 
               _sched.next_group(gid, _id); )
          if (_md.untested(gid))
            subset_gids.insert(gid);
        _unsat_guesses++;
        expecting_unsat = true;
      } else {
        // heuristics -- need to get the trace first
        if (config.get_verbosity() >= 3)
          cout_pref << "wrkr-" << _id << " analyzing a proof ... " << endl;            
        if (config.get_subset_mode() == 1) {
          // now, pick the subset with small counts -- take from the top of the queue
          assert(pcq != 0);
          while ((subset_gids.size() < config.get_subset_size()) && !pcq->empty()) {
            GID gid = pcq->top();
            if (_md.untested(gid)) {
              subset_gids.insert(gid);
              DBG(cout << "  added group " << gid << ": ";
                  const TraceAnalyzer::PathCountMap& pcm = ta.compute_path_count_map();
                  BasicClauseVector& cls = gset.gclauses(gid);
                  for (cvec_iterator pcl = cls.begin(); pcl != cls.end(); ++pcl) {
                    BasicClause* cl = *pcl;
                    cout << "clause " << *cl << ", ss_id=" << cl->ss_id() << ", path_count=";
                    TraceAnalyzer::PathCountMap::const_iterator pm = pcm.find(cl->ss_id()+1);
                    if (pm == pcm.end())
                      cout << "0(not there) "; // shouldn't happen
                    else          
                      cout << pm->second << " ";
                  }   
                  cout << endl;);
            }              
            pcq->pop();
          }
          _unsat_guesses++;
          expecting_unsat = true;
        } else if ((config.get_subset_mode() == 2) || (config.get_subset_mode() == 3)) {
          _analyzer_time -= RUSAGE::read_cpu_time();
          const TraceAnalyzer::ClauseSet& cs 
            = ta.compute_interesting_support((config.get_subset_mode() == 2), false);
          _analyzer_time += RUSAGE::read_cpu_time();
          DBG(cout << "Interesting support size = " << cs.size() << endl;);
          // TODO: fix this, this is very inefficient !!!
          for (cvec_iterator pcl = gset.begin(); pcl != gset.end(); ++pcl) {
            BasicClause* cl = *pcl;
            if (!cl->removed() && (cs.find(cl->ss_id()+1) != cs.end()) 
                && _md.untested(cl->get_grp_id()))
              subset_gids.insert(cl->get_grp_id());
          }
          if (subset_gids.empty()) {
            DBG(cout << "No untested in interesting support, picking one" << endl;);
            for (GID gid; _sched.next_group(gid, _id); )
              if (_md.untested(gid)) {
                subset_gids.insert(gid);
                break;
              }
          } else {
            _unsat_guesses++;
            expecting_unsat = true;
          }
        }
      }
      // if subset_gids is empty now, then we're done
      if (subset_gids.empty()) {
        DBG(cout << "No more groups to test." << endl;);
        break;
      }
      DBG(cout << "Got next subset, size = " << subset_gids.size() << ": " 
          << subset_gids << endl;);
    }
    if (config.get_verbosity() >= 3)
      cout_pref << "wrkr-" << _id << " checking gid subset " << subset_gids << " ... " << endl;
    // do the check
    if (tfile) {
      tfile->reset();
      _schecker.solver().set_proof_trace_stream(tfile->fstream());
    }
    _schecker.process(css);
    if (tfile) { 
      fprintf(tfile->fstream(), "-1\n"); // end of proof marker
      tfile->flush();
      tfile->rewind();
    }
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
      for (GIDSet::iterator pgid = subset_gids.begin(); 
           pgid != subset_gids.end(); ++pgid) {
        if (tv_group(css.model(), gset, *pgid) != 1) // undetermined cannot be assumed unsat
          false_gids.insert(*pgid);
        else if (config.get_subset_mode() == 0) // put the true ones back
          _sched.reschedule(*pgid);
        else if (config.get_subset_mode() == 1) { 
          // put the true ones back either into queue (if exists) or scheduler
          if (pcq)
            pcq->push(*pgid);
          else
            _sched.reschedule(*pgid);
        }
      }
      DBG(cout << "SAT: falsified gids " << false_gids << " ";);
      if (single_mode_gids.empty())
        ++((false_gids.size() == 1) ? _subset_singleton : _subset_nonsingleton);
      if (false_gids.size() == 1) {
        DBG(cout << "got a singleton, adding, rotating if needed." << endl;);
        // take care of the necessary group: put into MUSData, and mark final
        GID gid = *false_gids.begin();
        _md.nec_gids().insert(gid);
        _md.f_list().push_front(gid);
        // do rotation, if asked for it
        if (config.get_model_rotate_mode()) {
          rm.set_gid(gid);
          rm.set_model(css.model());
          rm.set_rot_depth(config.get_rotation_depth());
          rm.set_rot_width(config.get_rotation_width());
          rm.set_ignore_g0(config.get_ig0_mode());
          rm.set_ignore_global(config.get_iglob_mode());
          _mrotter.process(rm);
          if (rm.completed()) {
            unsigned r_count = 0;
            for (GIDSetIterator pgid = rm.nec_gids().begin();
                 pgid != rm.nec_gids().end(); ++pgid) {
              // double-check check if not necessary already and not gid 0
              if (*pgid && !_md.nec(*pgid)) {
                _md.nec_gids().insert(*pgid);
                _md.f_list().push_front(*pgid); 
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
        copy(false_gids.begin(), false_gids.end(), 
             inserter(single_mode_gids, single_mode_gids.begin()));
      }
      ++_sat_outcomes;
    } else { // gsc.status = UNSAT
      // take care of unnecessary groups
      GIDSet& ugids = css.unnec_gids();
      DBG(cout << "UNSAT: " << ugids.size() << " unnecessary groups." << endl;);
      if (!ugids.empty()) {
        for (GIDSetIterator pgid = ugids.begin(); pgid != ugids.end(); ++pgid) {
          _md.r_gids().insert(*pgid);
          _md.r_list().push_front(*pgid);
          // mark the clauses as removed (and update counts in the occlist)
          BasicClauseVector& clv = gset.gclauses(*pgid);
          for (cvec_iterator pcl = clv.begin(); pcl != clv.end(); ++pcl) {
            if (!(*pcl)->removed()) {
              (*pcl)->mark_removed();
              if (gset.has_occs_list())
                gset.occs_list().update_active_sizes(*pcl);
            }
          }
        }
      }
      ++_unsat_outcomes;
      _ref_groups += ugids.size() - subset_gids.size();
      if (expecting_unsat)
        ++_unsat_correct_guesses;
      // if reached the limit on the number of UNSAT outcomes (0 means no limit)
      // turn off subsetting
      if (_unsat_outcomes == config.get_unsat_limit()) {
        DBG(cout << "Reached the limit on UNSAT outcomes, turning off subsetting." << endl;);
        turned_off = true;
        if (tfile) {
          delete tfile; 
          tfile = 0;
        }
        _schecker.solver().set_proof_trace_stream(0);
      }
      if ((config.get_subset_mode() > 0) && !turned_off) {
        // pass the new trace to analyzer
        assert(tfile != 0);
        DBG(double t = _analyzer_time;);
        _analyzer_time -= RUSAGE::read_cpu_time();
        ta.set_trace_stream(tfile->fstream());
        _analyzer_time += RUSAGE::read_cpu_time();
        DBG(cout << "Time to parse: " << (_analyzer_time - t) << " sec" << endl;);
        has_trace = true;
        DBG(cout << "Have new trace, passed to analyzer ..." << endl;);
        if (config.get_subset_mode() == 1) {
          // get the path counts, and populate the queue
          _analyzer_time -= RUSAGE::read_cpu_time();
          const TraceAnalyzer::PathCountMap& pcm = ta.compute_path_count_map();
          if (config.get_verbosity() >= 3)
            cout_pref << "wrkr-" << _id << " analyzer read " << pcm.size()      
                      << " clauses" << endl;
          NDBG(cout << "Path counts:" << endl;
              for (TraceAnalyzer::PathCountMap::const_iterator pm = pcm.begin(); 
                   pm != pcm.end(); ++pm)
                cout << "    ss_id = " << (pm->first-1) << ", path_count = " << pm->second << endl;);
          _analyzer_time += RUSAGE::read_cpu_time();
          if (pcq)
            delete pcq;
          pcq = new PCQueue(PCOrder(pcm, gset, 1)); // 2 for largest first
          for (gset_iterator pg = gset.gbegin(); pg != gset.gend(); ++pg) {
            if ((*pg != 0) && pg.a_count())
              pcq->push(*pg);
          }
        }
      }
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
              << ", SAT guesses total(corr): " << _sat_guesses
              << "(" << _sat_correct_guesses << ")"
              << ", UNSAT guesses total(corr): " << _unsat_guesses 
              << "(" << _unsat_correct_guesses << ")";
    if (config.get_subset_mode() > 0)
      cout << ", analyzer time: " << _analyzer_time << " sec";
    cout << endl;
  }
  // some cleanup
  if (pcq)      
    delete pcq;
  if (tfile)
    delete tfile;
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


