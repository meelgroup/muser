/*----------------------------------------------------------------------------*\
 * File:        sat_checker.cc
 *
 * Description: Implementation of SAT checker worker.
 *
 * Author:      antonb
 * 
 * Notes:
 *
 *                                              Copyright (c) 2011, Anton Belov
\*----------------------------------------------------------------------------*/

#include <cassert>
#include <iostream>
#include <iterator>
#include "sat_checker.hh"

//#define DBG(x) x

using namespace std;

namespace {

  /* Creates a group which represents the CNF of a negation of the given group */
  void make_rr_group(const BasicGroupSet& gs, GID gid, BasicGroupSet& out_gs, 
                     GID out_gid);

}

/* Constructor makes an underlying instance of SAT solver based on the
 * values passed in the SATSolverConfig.
 */
SATChecker::SATChecker(IDManager& imgr, SATSolverConfig& config, unsigned id)
  : Worker(id), _imgr(imgr), _sfact(imgr), _config(config), 
    _psolver(&_sfact.instance(config)), _sat_calls(0)
{
  // initialize the solver
  _psolver->init_all();
}

SATChecker::~SATChecker(void)
{
  _psolver->reset_all();
  _sfact.release();
}

/* Handles the CheckGroupStatus work item by running a SAT check on the
 * appropriate instance
 */
bool SATChecker::process(CheckGroupStatus& gs)
{
  DBG(cout << "+SATChecker::process(CheckGroupStatus)" << endl;);
  const MUSData& md = gs.md();
  const GID& gid = gs.gid();
  // should not be checking group 0
  if (gid == 0) {
    assert(false);
    return false;
  }
  // synchronize
  sync_solver(md);

  // now, it is possible that by the time the worker got around to processing
  // of this work item, the status of the group has already been determined;
  // if this is the case get out
  if (md.r(gid) || md.nec(gid))
    return false;

  // deactivate the group
  _psolver->deactivate_group(gid);

  // handle redundancy removal if needed
  GID rr_gid = gid_Undef;
  if (gs.use_rr()) {
    BasicGroupSet rr_gs;
    make_rr_group(md.gset(), gid, rr_gs, rr_gid = (_psolver->max_gid() + 1));
    assert(!_psolver->exists_group(rr_gid));
    _psolver->add_groups(rr_gs);
  }

  // run SAT solver
  _psolver->init_run();
  _sat_time -= RUSAGE::read_cpu_time(); // TODO: not good for MT
  SATRes outcome = _psolver->solve();
  _sat_time += RUSAGE::read_cpu_time();
  _sat_calls++;

  // if UNSAT the group is unneccessary
  if (outcome == SAT_False) {
    // add groups outside of core, if asked for refinement
    if (gs.refine()) {
      refine(md, gs.unnec_gids(), rr_gid);
      gs.tainted_core() = gs.unnec_gids().empty();
    }
    gs.unnec_gids().insert(gid);
    gs.set_status(false);
    gs.set_completed();
  }
  // is SAT the gruop is necessary, save the model, if asked for it
  else if (outcome == SAT_True) {
    if (gs.need_model())
      _psolver->get_model(gs.model());
    gs.set_status(true);
    gs.set_completed();
  }
  // anything else - do nothing, the item is not completed

  // re-activate the group (to keep things in sync)
  _psolver->activate_group(gid);
  if (gs.use_rr()) {
    assert(rr_gid != gid_Undef);
    _psolver->del_group(rr_gid);
  }

  // done
  _psolver->reset_run();
  DBG(cout << "-SATChecker::process(CheckGroupStatus): " <<
      (gs.completed() ? (gs.status() ? "nec" : "unnec") : "unknown") << endl;);
  return gs.completed();
}


/* Handles the TrimGroupSet work item
 */
bool SATChecker::process(TrimGroupSet& tg)
{
  DBG(cout << "+SATChecker::process(TrimGroupSet): triming ..." << endl;);

  MUSData& md = tg.md();

  // the trimming loop: to stop we need to know the number of iterations 
  // and the previous size; note that we bypass all the high-level methods
  // such as sync and refine for efficiency -- once everything is finished
  // we will update the MUSData with the final result
  /* const */BasicGroupSet& gs = md.gset();
  unsigned prev_size = gs.gsize();
  unsigned num_iter = 0;
  GIDSet trimmed_gids;  // the GIDs of trimmed away groups
  while (1) {
    ++num_iter;
    DBG(cout << "  iteration " << num_iter << ", prev_size=" 
        << prev_size << endl;);
    // do the sync ...
    sync_solver(md);
    // run solver
    _psolver->init_run();
    _sat_time -= RUSAGE::read_cpu_time(); // TODO: not good for MT
    SATRes outcome = _psolver->solve();
    _sat_time += RUSAGE::read_cpu_time(); // TODO: not good for MT
    _sat_calls++;
    if (outcome == SAT_True) {
      DBG(cout << "  instance is SAT, terminating trimming." << endl;);
      break; // is_unsat will stay false
    }
    tg.set_unsat();
    // refine: every (non-removed) group that is not in the core is removed, and
    // saved inside trimmed_gids
    GIDSet& gcore = _psolver->get_group_unsat_core();
    unsigned r_count = 0;
    for (gset_iterator pgid = gs.gbegin(); pgid != gs.gend(); ++pgid) {
      if ((*pgid != 0) &&
          (trimmed_gids.find(*pgid) == trimmed_gids.end()) && // not already trimmed
          (gcore.find(*pgid) == gcore.end())) { // not in the core
        trimmed_gids.insert(*pgid);
        // update the MUSData right away - for re-initializing the solver
        md.r_gids().insert(*pgid);
        md.r_list().push_front(*pgid);
        // mark the clauses as removed (and update counts in the occlist)
        BasicClauseVector& clv = gs.gclauses(*pgid);
        for (cvec_iterator pcl = clv.begin(); pcl != clv.end(); ++pcl) {
          if (!(*pcl)->removed()) {
            (*pcl)->mark_removed();
            if (gs.has_occs_list())
              gs.occs_list().update_active_sizes(*pcl);
          }
        }
        if (_psolver->exists_group(*pgid))
          _psolver->del_group(*pgid);
        r_count++;
      }
    }
    _psolver->reset_run();
    DBG(cout << "  iteration finished, removed " << r_count << " groups." << endl;);
    // check termination
    if (r_count == 0) {
        DBG(cout << "  fixpoint is reached, terminating trimming." << endl;);
        break;
    } else if (!tg.trim_fixpoint()) {
      if (tg.iter_limit() > 0) {
        if (num_iter >= tg.iter_limit()) {
          DBG(cout << "  maximum number of iterations reached, terminating trimming." 
              << endl;);
          break;
        }
      } else if (tg.pct_limit() > 0) {
        if (r_count < ((float)prev_size * tg.pct_limit() / 100)) {
          DBG(cout << "  reduction is less than required, terminating trimming." 
              << endl;);  
          break;
        }
      } else {
        tool_abort("invalid trimming configuration.");
      }
    }
    // update size
    prev_size -= r_count;
    // if going for the next iteration, drop the current solver, make a new one
    _psolver->reset_all();
    _sfact.release();
    _psolver = &_sfact.instance(_config);
    _psolver->init_all();
  }
  // done -- update the MUSData
  DBG(cout << "-SATChecker::process(TrimGroupSet): trimming is finished, removed " 
      << trimmed_gids.size() << " groups/clauses." << endl;);
  tg.set_completed();
  return tg.completed();
}


/* Handles the CheckUnsat work item by simply invoking the SAT solver, and
 * checking the result 
 */
bool SATChecker::process(CheckUnsat& cu) 
{
  const MUSData& md = cu.md();

  sync_solver(md);

  // run SAT solver
  _psolver->init_run();
  _sat_time -= RUSAGE::read_cpu_time(); // TODO: not good for MT
  if (_psolver->solve() == SAT_False)
    cu.set_unsat();
  _sat_time += RUSAGE::read_cpu_time(); // TODO: not good for MT
  _psolver->reset_run();
  _sat_calls++;

  // done
  cu.set_completed();   
  return cu.completed();
}

/* Handles the CheckRangeStatus work item by running a SAT check on the
 * appropriate instance.
 */
bool SATChecker::process(CheckRangeStatus& crs)
{
  DBG(cout << "+" << __PRETTY_FUNCTION__ << endl;);
  const MUSData& md = crs.md();
  BasicGroupSet& gset(*const_cast<BasicGroupSet*>(&md.gset()));

  // custom synchronization (refactor)
  if (_psolver->gsize() == 0) {
    // load the necessary groups -- make them final right away
    for (auto gid : md.nec_gids())
      _psolver->add_group(gset, gid, true);
    // add negation of the rest, and populate _aux_map, unless its been done already
    if (crs.add_negation() && _aux_map.empty()) {
      DBG(cout << "  Adding negation clauses: ";);
      vector<LINT> lits(2);
      for_each(crs.begin(), crs.allend(), [&](GID gid) {
          assert(gset.gclauses(gid).size() == 1);
          ULINT aux_var = _imgr.new_id();
          _aux_map.insert({ gid, aux_var });
          const BasicClause* cl = *gset.gclauses(gid).begin();
          for_each(cl->abegin(), cl->aend(), [&](LINT lit) {
              lits[0] = -lit;
              lits[1] = -aux_var;
              BasicClause* new_cl = gset.make_clause(lits, 0);
              _psolver->add_final_clause(new_cl);
              DBG(cout << *new_cl << " ");
              gset.destroy_clause(new_cl);
            });
        });
      // long clause (also final)
      lits.clear();
      for (auto& ga : _aux_map)
        lits.push_back(ga.second);
      BasicClause* new_cl = gset.make_clause(lits, 0);
      _psolver->add_final_clause(new_cl);
      DBG(cout << "long clause: " << *new_cl << endl;);
      gset.destroy_clause(new_cl);
    }
  } else {
    // synchronize: remove removed groups, finalize new groups, remove their 
    // negations
    for (auto gid : md.r_list()) {
      if (_psolver->exists_group(gid)) // some may not have been added yet
        _psolver->del_group(gid);
    }
    for (auto gid : md.f_list()) {
      if (_psolver->exists_group(gid)) // some may not have been added yet
        _psolver->make_group_final(gid);
      else
        _psolver->add_group(gset, gid, true);
    }
    if (crs.add_negation()) {
      // permanently remove negations of deleted and finalized gruops
      DBG(cout << "  Assering units: ";);
      for (auto gid : md.r_list()) {
        assert(_aux_map.count(gid));
        _psolver->add_final_unit_clause(-_aux_map[gid]);
        DBG(cout << -_aux_map[gid] << " ";);
      }
      for (auto gid : md.f_list()) {
        assert(_aux_map.count(gid));
        _psolver->add_final_unit_clause(-_aux_map[gid]);
        DBG(cout << -_aux_map[gid] << " ";);
      }
      DBG(cout << endl;);
    }
  }
  
  // load or activate the groups in [begin, end), and deactivate any existing 
  // groups in [end, all_end) (some groups may have not been added)
  for_each(crs.begin(), crs.end(), [&](GID gid) { 
      if (_psolver->exists_group(gid)) {
        if (!_psolver->is_group_active(gid))
          _psolver->activate_group(gid);
      } else
        _psolver->add_group(gset, gid);
    });
  for_each(crs.end(), crs.allend(), [&](GID gid) {
      if (_psolver->exists_group(gid) && _psolver->is_group_active(gid))
        _psolver->deactivate_group(gid);
    });

  // run SAT solver
  SATRes outcome = SAT_NoRes;
  _psolver->init_run();
  _sat_time -= RUSAGE::read_cpu_time(); // TODO: not good for MT
  if (crs.add_negation()) {
    // the auxilliary literals of groups in [begin, end) will be passed as 
    // assumptions in order to temporary disable them;
    IntVector assum;
    for_each(crs.begin(), crs.end(), [&](GID gid) {
        assert(_aux_map.count(gid));
        assum.push_back(-_aux_map[gid]); });
    DBG(cout << "  Assumptions to the solver: "; PRINT_ELEMENTS(assum););
    outcome = _psolver->solve(assum);
  } else
    outcome = _psolver->solve();
  _sat_time += RUSAGE::read_cpu_time();
  _sat_calls++;

  // if UNSAT, collect unnecessary groups
  if (outcome == SAT_False) {
    if (crs.refine()) {
      // add groups from [begin, end) that are not in the core
      GIDSet& gcore = _psolver->get_group_unsat_core();
      copy_if(crs.begin(), crs.end(), inserter(crs.unnec_gids(), crs.unnec_gids().begin()), 
              [&](GID gid) { return !gcore.count(gid); });
    }
    crs.set_status(false);
    crs.set_completed();
  }
  // is SAT save the model, if asked for it
  else if (outcome == SAT_True) {
    if (crs.need_model())
      _psolver->get_model(crs.model());
    crs.set_status(true);
    crs.set_completed();
  }

  // done
  _psolver->reset_run();

  DBG(cout << "-" << __PRETTY_FUNCTION__ << ": " <<
      (crs.completed() ? (crs.status() ? "SAT" : "UNSAT") : "UNKNOWN") << endl;);
  return crs.completed();
}



////////////////////////////////////////////////////////////////////////////////

/* Loads the groupset into the SAT solver. This methods expects that the SAT
 * solver is empty. The removed groups will not be added, and the final groups
 * will be finalized.
 */
void SATChecker::load_groupset(const MUSData& md)
{
  if (_psolver->gsize() > 0)
    throw logic_error(string(__PRETTY_FUNCTION__)+": called on non-empty solver.");

  // TEMP: get rid of const - TODO: fix when BasicGroupSet is const-correct
  BasicGroupSet& gs(*const_cast<BasicGroupSet*>(&md.gset()));
  _psolver->add_groups(gs);
  for (auto gid : md.r_list()) {
    assert(!_psolver->exists_group(gid)); // should not have been added at all !
    if (_psolver->exists_group(gid)) // remove this once tested
      _psolver->del_group(gid);
  }
  for (auto gid : md.f_list()) 
    _psolver->make_group_final(gid);
}


/* Synchronizes the SAT solver with the current state of MUS data; no locking
 * is done in this method, however it reads from 'md'
 * Notes:
 *   1. The currently supported cases must fall into one of the following
 *   categories:
 *   (a) The SAT solver has no groups. In this case the group set from md.gset()
 *   is loaded into the solver, and the groups from md.f_list() and md.r_list()
 *   are processed (finalized resp. removed)
 *   (b) The SAT solver has groups, and md.gset() \setminus md.r_gids() is a
 *   subset of the groups in the solver. This check is currently performed
 *   solely based on the size of the sets. In this case, the groups newly
 *   listed in md.r_list() are removed. In addition, the groups newly listed
 *   in md.f_list() are made final. Note that both lists are assumed to be
 *   kept in the order from most recent to least recent, so we terminate early
 *   the moment we hit a clause that already has been removed/final.
 */
void SATChecker::sync_solver(const MUSData& md)
{
  // TEMP: get rid of const - TODO: fix when BasicGroupSet is const-correct
  BasicGroupSet& gs(*const_cast<BasicGroupSet*>(&md.gset()));

  // case (a): SAT solver empty -- load all groups into solver; remove removed
  // groups, finalize the final groups
  if (_psolver->gsize() == 0) {
    _psolver->add_groups(gs);
    for (GIDListCIterator pg = md.r_list().begin(); pg != md.r_list().end(); ++pg)
      if (_psolver->exists_group(*pg)) // need this b/c of pre-processing - the group
        _psolver->del_group(*pg);      // might be removed already
    for (GIDListCIterator pg = md.f_list().begin(); pg != md.f_list().end(); ++pg)
      _psolver->make_group_final(*pg);
  }
  // case (b): assume that some clauses are gone from gset
  else if (gs.gsize() - md.r_gids().size() <= (ULINT)_psolver->gsize()) {
    // scan the list from the front and for each clause remove it from the
    // solver -- if a clause is not in the solver, stop
    for (GIDListCIterator pg = md.r_list().begin(); pg != md.r_list().end(); ++pg) {
      if (_psolver->exists_group(*pg))
        _psolver->del_group(*pg);
      else
        break;
    }
    // same for final clauses
    for (GIDListCIterator pg = md.f_list().begin(); pg != md.f_list().end(); ++pg) {
      if (!_psolver->is_group_final(*pg))
        _psolver->make_group_final(*pg);
      else
        break;
    }
  }
  // unsupported case
  else {
    DBG(cout << "SATChecker::sync_solver: unsupported case gs.gsize() = "
             << gs.gsize() << ", removed " << md.r_gids().size()
             << ", solver gsize = " << _psolver->gsize() << endl;)
    assert(false);
  }
}

/* In the case the last SAT call returned UNSAT, this method will get the core
 * from the SAT solver, and will add the GIDs of unnecessary groups (i.e. those
 * none of whose clauses are in the core) into unnec_gids; reads from md.
 * The third parameter, rr_gid, if not gid_Undef, specifies the gid of the group
 * used for redundancy removal trick -- if the core contains rr_gid, then the
 * refinement cannot be used safely, in this cases unnec_gids stays empty.
 */
void SATChecker::refine(const MUSData& md, GIDSet& unnec_gids, GID rr_gid)
{
  // TEMP: get rid of const - TODO: fix when BasicGroupSet is const-correct
  BasicGroupSet& gs(*const_cast<BasicGroupSet*>(&md.gset()));

  // put all unremoved and not known to be necessary groups that are not in the
  // group core into unnec_gids
  GIDSet& gcore = _psolver->get_group_unsat_core();
  if ((rr_gid == gid_Undef) || (gcore.find(rr_gid) == gcore.end())) {
    // refinement is safe
    for (gset_iterator pgid = gs.gbegin(); pgid != gs.gend(); ++pgid) {
      if ((*pgid != 0) && !md.r(*pgid) && !md.nec(*pgid) // this last test is redundant
          && (gcore.find(*pgid) == gcore.end()))
        unnec_gids.insert(*pgid);
    }
    DBG(cout << "=SATChecker::refine() - core is clean, refined." << endl;);
  } else {
    DBG(cout << "=SATChecker::refine() - core is tainted, not refined." << endl;);
  }
  // done
}

//
// ------------------------  Local implementations  ----------------------------
//

namespace {

  /* Creates a group which represents the CNF of a negation of the clauses in
   * the given group, and adds to the specified group-set; the group will have
   * the GID of out_gid
   */
  void make_rr_group(const BasicGroupSet& gs, GID gid, BasicGroupSet& out_gs, 
                     GID out_gid) 
  {
    if (gs.a_count(gid) != 1) // TEMP: only for CNF case
      throw std::logic_error("redundancy removal is not yet implemented for "
                             "non-singleton groups");
    BasicClause* src_cl = *(gs.gclauses(gid).begin());
    DBG(cout << "Source clause: " << *src_cl << endl;);
    // take care of an empty clause: make a fake tautology
    if (src_cl->asize() == 0) {
      vector<LINT> lits;        
      lits.push_back(1);
      lits.push_back(-1);
      BasicClause* new_cl = out_gs.create_clause(lits);
      out_gs.set_cl_grp_id(new_cl, out_gid);
    } else {
      for (CLiterator plit = src_cl->abegin(); plit != src_cl->aend(); ++plit) {
        vector<LINT> lits;        
        lits.push_back(-*plit);
        BasicClause* new_cl = out_gs.create_clause(lits);
        out_gs.set_cl_grp_id(new_cl, out_gid);
      }
    }
    DBG(cout << "Out group set: " << endl; out_gs.dump(););
  }

}
