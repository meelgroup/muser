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
#include "utils.hh"

//#define DBG(x) x
//#define CHK(x) x

using namespace std;
using MUSer2::SATSolverWrapper;

namespace {

  /* Creates a group which represents the CNF of a negation of the given group */
  void make_rr_group(const BasicGroupSet& gs, GID gid, BasicGroupSet& out_gs, 
                     GID out_gid, IDManager& imgr);
  /* Returns true if a variable group exists in the solver */
  bool exists_vgroup(SATSolverWrapper* psolver, BasicGroupSet& gs, GID vgid);
  /* Returns true if a variable group is final */
  bool is_vgroup_final(SATSolverWrapper* psolver, BasicGroupSet& gs, GID vgid);
  /* Deletes variable group from the solver */
  void del_vgroup(SATSolverWrapper* psolver, BasicGroupSet& gs, GID vgid);
  /* Finalizes variable group in the solver */
  void make_vgroup_final(SATSolverWrapper* psolver, BasicGroupSet& gs, GID vgid);
  /* Deactivates variable group in the solver */
  void deactivate_vgroup(SATSolverWrapper* psolver, BasicGroupSet& gs, GID vgid);
  /* Activates variable group in the solver */
  void activate_vgroup(SATSolverWrapper* psolver, BasicGroupSet& gs, GID vgid);
  /* Collects clauses of VGID */
  void collect_clauses(BasicGroupSet& gs, GID vgid, BasicClauseVector& clvec);
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

  // grab read-lock
  md.lock_for_reading();
  // synchronize
  sync_solver(md);
  // remember the version
  gs.set_version(md.version());

  // now, it is possible that by the time the worker got around to processing
  // of this work item, the status of the group has already been determined;
  // if this is the case, set the flag, and get out
  bool status_known = false;
  if (md.r(gid)) {
    //cout << " already removed. Skipping." << endl;
    status_known = true;
  } else if (md.nec(gid)) {
    //cout << " already known to be necessary. Skipping." << endl;
    status_known = true;
  }
  // release lock
  md.release_lock();
  // nothing to do if the status is already known
  if (status_known)
    return false;

  // deactivate the group
  _psolver->deactivate_group(gid);

  // handle redundancy removal if needed
  GID rr_gid = gid_Undef;
  if (gs.use_rr()) {
    BasicGroupSet rr_gs;
    Utils::make_neg_group(md.gset().gclauses(gid), rr_gs, rr_gid = (_psolver->max_gid() + 1), _imgr);
    assert(!_psolver->exists_group(rr_gid));
    _psolver->add_groups(rr_gs);
  }

  // for proof-checker instances, set the polarity of abvreviations
  ToolConfig* pcfg = static_cast<ToolConfig*>(&_config);
  if (pcfg->get_pc_mode() && pcfg->get_pc_pol()) {
    for (ULINT av = md.gset().get_first_abbr(); av < md.gset().get_first_sel(); ++av)
      _psolver->set_phase(av, (pcfg->get_pc_pol() == 1) ? 1 : 0);
  }
  // set limits, if specified
  if (gs.conf_limit() != -1) { _psolver->set_max_conflicts(gs.conf_limit()); }
  if (gs.cpu_limit() != 0.0f) { _psolver->set_timeout(gs.cpu_limit()); }
  // run SAT solver
  _psolver->init_run();
  SATRes outcome = solve();

  // if UNSAT the group is unneccessary
  if (outcome == SAT_False) {
    // add groups outside of core, if asked for refinement
    if (gs.refine()) {
      md.lock_for_reading();
      refine(md, gs.unnec_gids(), rr_gid);
      gs.tainted_core() = gs.unnec_gids().empty();
      md.release_lock();
    }
    gs.unnec_gids().insert(gid);
    if (gs.save_core()) { gs.set_pcore(&_psolver->get_group_unsat_core()); }
    gs.set_status(false);
    gs.set_completed();
  }
  // is SAT the gruop is necessary, save the model, if asked for it
  else if (outcome == SAT_True) {
    if (gs.need_model())
      _psolver->get_model(gs.model());
    if (gs.model().size() <= md.gset().max_var())
      gs.model().resize(md.gset().max_var() + 1, 0);
    gs.set_pcore(nullptr);
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
    md.lock_for_reading();
    sync_solver(md);
    md.release_lock();
    // run solver
    _psolver->init_run();
    SATRes outcome = solve();
    if (outcome == SAT_True) {
      DBG(cout << "  instance is SAT, terminating trimming." << endl;);
      break; // is_unsat will stay false
    }
    tg.set_unsat();
    // refine: every (non-removed) group that is not in the core is removed, and
    // saved inside trimmed_gids
    GIDSet& gcore = _psolver->get_group_unsat_core();
    unsigned r_count = 0;
    md.lock_for_writing(); // will update MUSData right away
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
    md.incr_version();
    md.release_lock();
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

  // grab read-lock, sync, release
  md.lock_for_reading();
  sync_solver(md);
  md.release_lock();

  // run SAT solver
  _psolver->init_run();
  if (solve() == SAT_False) { cu.set_unsat(); }
  _psolver->reset_run();

  // done
  cu.set_completed();   
  return cu.completed();
}


/* Handles the CheckGroupStatusChunk work item
 */
bool SATChecker::process(CheckGroupStatusChunk& gsc)
{
  DBG(cout << "+SATChecker::process(CheckGroupStatusChunk)" << endl;);
  const MUSData& md = gsc.md();
  // TEMP: get rid of const - TODO: fix when BasicGroupSet is const-correct
  BasicGroupSet& gset(*const_cast<BasicGroupSet*>(&md.gset()));
  const GID& gid = gsc.gid();
  const GIDSet& chunk = gsc.chunk();
  // should not be checking group 0 and group has to be in the chunk
  assert((gid != 0) && (chunk.find(gid) != chunk.end()));
  if ((gid == 0) || (chunk.find(gid) == chunk.end())) {
    return false;
  }
  // grab read-lock
  md.lock_for_reading();
  // synchronize
  sync_solver(md);
  // remember the version
  gsc.set_version(md.version());
  // now, it is possible that by the time the worker got around to processing
  // of this work item, the status of the group has already been determined;
  // if this is the case, set the flag, and get out
  bool status_known = md.r(gid) || md.nec(gid);
  // release lock
  md.release_lock();
  // nothing to do if the status is already known
  if (status_known)
    return false;

  // if this is first call for this chunk - load the negation of the chunk 
  // into solver, but drop the previous one beforehand
  if (gsc.first()) {
    DBG(cout << "  Started new chunk ..." << endl;);
    if (!_aux_map.empty()) {
      // in the presence of immediate-removal optimization, this should be empty
      DBG(cout << "  Dropping the left-over of negation of previous chunk, gids: ";);
      vector<LINT> unit;
      unit.resize(1);
      for (GID2IntMap::iterator p = _aux_map.begin(); p != _aux_map.end(); ++p) {
        DBG(cout << p->first << "(aux=" << p->second << ") ";);
        unit[0] = -p->second;
        BasicClause* ucl = gset.make_clause(unit, 0);
        _psolver->add_final_clause(ucl);
        gset.destroy_clause(ucl);
      }
      DBG(cout << "done." << endl;);    
      _aux_map.clear();
    }
    if (_aux_long_gid != gid_Undef) {
      DBG(cout << "  Dropping long clause, gid = " << _aux_long_gid << endl;);
      assert(_psolver->exists_group(_aux_long_gid));
      _psolver->del_group(_aux_long_gid);
      _aux_long_gid = gid_Undef;
    }
    // add clauses that will contain the PG transform of the chunk: for each 
    // clause Ci = (l1,...,lk) make a new auxiliary variable ai, and add the
    // clauses (-ai,-l1) ... (-ai,-lk) to the solver. Then, add a clause
    // (a1,...an) to complete the transform.
    vector<LINT> lits;
    lits.resize(2); // binary clauses
    assert(_aux_map.empty());
    DBG(cout << "  Adding negation clauses: ";);
    for (GIDSet::const_iterator pgid = chunk.begin(); pgid != chunk.end(); ++pgid) {
      assert(gset.gclauses(*pgid).size() == 1);
      ULINT aux_var = _imgr.new_id();
      _aux_map.insert(make_pair(*pgid, aux_var));
      const BasicClause* cl = *gset.gclauses(*pgid).begin();
      for (CLiterator plit = cl->abegin(); plit != cl->aend(); ++plit) {
        lits[0] = -*plit;
        lits[1] = -aux_var;
        BasicClause* new_cl = gset.make_clause(lits, 0);
        _psolver->add_final_clause(new_cl);
        DBG(cout << *new_cl << " ");
        gset.destroy_clause(new_cl);
      }
    }
    lits.clear();
    // add the long clause as a groupset
    for (GID2IntMap::iterator p = _aux_map.begin(); p != _aux_map.end(); ++p)
      lits.push_back(p->second);
    BasicGroupSet lgs;
    BasicClause* new_cl = lgs.create_clause(lits, 0);
    lgs.set_cl_grp_id(new_cl, _aux_long_gid = (_psolver->max_gid() + 1));
    assert(!_psolver->exists_group(_aux_long_gid));
    _psolver->add_groups(lgs);
    DBG(cout << " long clause: " << *new_cl << "(gid = " << _aux_long_gid 
        << ")" << endl;);
  }
    
  // ok, all set, do the usual deal ...

  // deactivate the group
  _psolver->deactivate_group(gid);

  // run SAT solver
  _psolver->init_run();
  SATRes outcome = solve();

  // if UNSAT the group is unneccessary
  if (outcome == SAT_False) {
    // add groups outside of core, if asked for refinement
    if (gsc.refine()) {
      md.lock_for_reading();
      // customized version of refinement -- only considers chunk gids
      GIDSet& gcore = _psolver->get_group_unsat_core();
      for (GIDSet::const_iterator pgid = chunk.begin(); pgid != chunk.end(); ++pgid) {
        if (!md.r(*pgid) && !md.nec(*pgid) && (gcore.find(*pgid) == gcore.end()))
          gsc.unnec_gids().insert(*pgid);
      }
      md.release_lock();
    }
    gsc.unnec_gids().insert(gid);
    gsc.set_status(false);
    gsc.set_completed();
  }
  // is SAT the group is necessary, save the model, if asked for it
  else if (outcome == SAT_True) {
    if (gsc.need_model())
      _psolver->get_model(gsc.model());
    if (gsc.model().size() <= md.gset().max_var())
      gsc.model().resize(md.gset().max_var() + 1, 0);
    gsc.set_status(true);
    gsc.set_completed();
  }
  // anything else - do nothing, the item is not completed

  // re-activate the group (to keep things in sync)
  _psolver->activate_group(gid);

  // done
  _psolver->reset_run();

  DBG(cout << "-SATChecker::process(CheckGroupStatusChunk): " <<
      (gsc.completed() ? (gsc.status() ? "nec" : "unnec") : "unknown") << endl;);
  return gsc.completed();
}


/* Handles the CheckVGroupStatus work item by running a SAT check on the
 * appropriate instance
 */
bool SATChecker::process(CheckVGroupStatus& vgs)
{
  DBG(cout << "+SATChecker::process(CheckVGroupStatus)" << endl;);
  const MUSData& md = vgs.md();
  BasicGroupSet& gset(*const_cast<BasicGroupSet*>(&md.gset()));
  const GID& vgid = vgs.vgid();
  // should not be checking group 0
  if (vgid == 0) {
    assert(false);
    return false;
  }

  // grab read-lock
  md.lock_for_reading();
  // synchronize
  vsync_solver(md);
  // remember the version
  vgs.set_version(md.version());

  // now, it is possible that by the time the worker got around to processing
  // of this work item, the status of the group has already been determined;
  // if this is the case, set the flag, and get out
  bool status_known = false;
  if (md.r(vgid)) {
    status_known = true;
  } else if (md.nec(vgid)) {
    status_known = true;
  }
  // release lock
  md.release_lock();
  // nothing to do if the status is already known
  if (status_known)
    return false;

  // handle redundancy removal, if asked for ...
  GID rr_gid = gid_Undef;
  if (vgs.use_rr()) { // TODO: refactor
    rr_gid = (_psolver->max_gid() + 1);
    assert(!_psolver->exists_group(rr_gid));
    BasicGroupSet rr_gs;
    BasicClauseVector vgclauses;
    collect_clauses(gset, vgid, vgclauses);
    DBG(cout << "  VGroup clauses:"; PRINT_PTR_ELEMENTS(vgclauses););
    // add clauses that will contain the PG transform of the conjunction of the
    // clauses: for each clause Ci = (l1,...,lk) make a new auxiliary variable 
    // ai, and add the clauses (-ai,-l1) ... (-ai,-lk) to the solver. Then, add 
    // a "long" clause (a1,...an) to complete the transform.
    vector<LINT> lits, long_lits;
    lits.resize(2); // binary clauses
    DBG(cout << "  Adding negation clauses, rr_gid = " << rr_gid << " : ";);
    for (BasicClauseVector::iterator pcl = vgclauses.begin(); pcl != vgclauses.end(); ++pcl) {
      const BasicClause* cl = *pcl;
      ULINT aux_var = _imgr.new_id();
      for (CLiterator plit = cl->abegin(); plit != cl->aend(); ++plit) {
        lits[0] = -*plit;
        lits[1] = -aux_var;
        BasicClause* new_cl = rr_gs.create_clause(lits);
        rr_gs.set_cl_grp_id(new_cl, rr_gid);
        DBG(cout << *new_cl << " ");
      }
      long_lits.push_back(aux_var);
    }
    // add the long clause as a groupset
    BasicClause* long_cl = rr_gs.create_clause(long_lits);
    rr_gs.set_cl_grp_id(long_cl, rr_gid);
    DBG(cout << " long clause: " << *long_cl << endl;);
    _psolver->add_groups(rr_gs);
  }

  // deactivate the group
  deactivate_vgroup(_psolver, gset, vgid);

  // run SAT solver
  _psolver->init_run();
  SATRes outcome = solve();

  // if UNSAT the group is unneccessary
  if (outcome == SAT_False) {
    // add groups outside of core, if asked for refinement
    if (vgs.refine()) {
      md.lock_for_reading();
      vrefine(md, vgs.unnec_vgids(), vgs.ft_vgids(), rr_gid);
      md.release_lock();
    }
    vgs.unnec_vgids().insert(vgid);
    vgs.set_status(false);
    vgs.set_completed();
  }
  // is SAT the gruop is necessary, save the model, if asked for it
  else if (outcome == SAT_True) {
    if (vgs.need_model())
      _psolver->get_model(vgs.model());
    if (vgs.model().size() <= md.gset().max_var())
      vgs.model().resize(md.gset().max_var() + 1, 0);
    vgs.set_status(true);
    vgs.set_completed();
  }
  // anything else - do nothing, the item is not completed

  // re-activate the group (to keep things in sync)
  activate_vgroup(_psolver, gset, vgid);
  if (vgs.use_rr()) {
    assert(rr_gid != gid_Undef);
    _psolver->del_group(rr_gid);
  }

  // done
  _psolver->reset_run();
  DBG(cout << "-SATChecker::process(CheckVGroupStatus): " <<
      (vgs.completed() ? (vgs.status() ? "nec" : "unnec") : "unknown") << endl;);
  return vgs.completed();
}


/* Handles the CheckSubsetStatus work item by running a SAT check on the
 * appropriate instance
 */
bool SATChecker::process(CheckSubsetStatus& css)
{
  DBG(cout << "+SATChecker::process(CheckSubsetStatus)" << endl;);
  const MUSData& md = css.md();
  const GIDSet& gids = css.subset();
  // should not be checking group 0
  if (gids.find(0) != gids.end()) {
    assert(false);
    return false;
  }     

  // grab read-lock
  md.lock_for_reading();
  // synchronize
  sync_solver(md);
  // remember the version
  css.set_version(md.version());

  // now, it is possible that by the time the worker got around to processing
  // of this work item, the status of one or more groups has already been 
  // determined; if this is the case, set the flag, and get out; this shouldn't
  // really ever happen in a singlethreaded mode
  bool status_known = false;
  for (GIDSet::const_iterator pgid = gids.begin(); pgid != gids.end(); ++pgid)
    if (md.r(*pgid) || md.nec(*pgid)) {
      status_known = true;
      break;
    }
  // release lock
  md.release_lock();
  // nothing to do if the status is already known
  if (status_known)
    return false;

  // deactivate the groups
  for (GIDSet::const_iterator pgid = gids.begin(); pgid != gids.end(); ++pgid)
    _psolver->deactivate_group(*pgid);

  // handle redundancy removal if needed
  GID rr_gid = gid_Undef;
  if (css.use_rr()) {
#if 0 // +TEMP
    BasicGroupSet rr_gs;
    make_rr_group(md.gset(), gid, rr_gs, rr_gid = (_psolver->max_gid() + 1));
    assert(!_psolver->exists_group(rr_gid));
    _psolver->add_groups(rr_gs);
#endif // -TEMP
  }

  // run SAT solver
  _psolver->init_run();
  SATRes outcome = solve();

  // if UNSAT the group is unneccessary
  if (outcome == SAT_False) {
    // add groups outside of core, if asked for refinement
    if (css.refine()) {
      md.lock_for_reading();
      refine(md, css.unnec_gids(), rr_gid);
      md.release_lock();
    } else {
      copy(gids.begin(), gids.end(), 
           inserter(css.unnec_gids(), css.unnec_gids().begin()));
    }
    css.set_status(false);
    css.set_completed();
  }
  // is SAT the gruop is necessary, save the model, if asked for it
  else if (outcome == SAT_True) {
    if (css.need_model())
      _psolver->get_model(css.model());
    if (css.model().size() <= md.gset().max_var())
      css.model().resize(md.gset().max_var() + 1, 0);
    css.set_status(true);
    css.set_completed();
  }
  // anything else - do nothing, the item is not completed

  // re-activate the groups (to keep things in sync)
  for (GIDSet::const_iterator pgid = gids.begin(); pgid != gids.end(); ++pgid)
    _psolver->activate_group(*pgid);
  if (css.use_rr()) {
    assert(rr_gid != gid_Undef);
    _psolver->del_group(rr_gid);
  }

  // done
  _psolver->reset_run();
  DBG(cout << "-SATChecker::process(CheckSubsetStatus): " <<
      (css.completed() ? (css.status() ? "nec" : "unnec") : "unknown") << endl;);
  return css.completed();
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
  md.lock_for_reading();
  if (_psolver->gsize() == 0) {
    // load group 0
    if (gset.has_g0())
      _psolver->add_group(gset, 0, true);
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
  md.release_lock();
  
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
      // to avoid problems with simplification, always add non-existing groups
      if (_psolver->is_preprocessing() && !_psolver->exists_group(gid))
        _psolver->add_group(gset, gid);
      if (_psolver->exists_group(gid) && _psolver->is_group_active(gid))
        _psolver->deactivate_group(gid);
    });

  // run SAT solver
  _psolver->init_run();
  SATRes outcome = SAT_NoRes;
  if (crs.add_negation()) {
    // the auxilliary literals of groups in [begin, end) will be passed as 
    // assumptions in order to temporary disable them;
    IntVector assum;
    for_each(crs.begin(), crs.end(), [&](GID gid) {
        assert(_aux_map.count(gid));
        assum.push_back(-_aux_map[gid]); });
    DBG(cout << "  Assumptions to the solver: "; PRINT_ELEMENTS(assum););
    outcome = solve(&assum);
  } else
    outcome = solve();

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
    if (crs.model().size() <= md.gset().max_var())
      crs.model().resize(md.gset().max_var() + 1, 0);
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
  // groups (this is possile in multi-threaded environment), finalize the final
  // groups
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
      if (_psolver->exists_group(*pg)) {
        _psolver->del_group(*pg);
        // optimization: the aux mapping for group, if there, should be gone too
        GID2IntMap::iterator pm = _aux_map.find(*pg);
        if (pm != _aux_map.end()) {
          vector<LINT> unit(1, -pm->second);
          BasicClause *ucl = gs.make_clause(unit, 0);
          _psolver->add_final_clause(ucl);
          gs.destroy_clause(ucl);
          _aux_map.erase(pm);
        }
      } else
        break;
    }
    // same for final clauses
    for (GIDListCIterator pg = md.f_list().begin(); pg != md.f_list().end(); ++pg) {
      if (!_psolver->is_group_final(*pg)) {
        _psolver->make_group_final(*pg);
        // same optimization
        GID2IntMap::iterator pm = _aux_map.find(*pg);
        if (pm != _aux_map.end()) {
          vector<LINT> unit(1, -pm->second);
          BasicClause *ucl = gs.make_clause(unit, 0);
          _psolver->add_final_clause(ucl);
          gs.destroy_clause(ucl);
          _aux_map.erase(pm);
        }
      } else
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

/* Invokes the underlying SAT solver; assumes that solver->init_run() has been
 * invoked
 */
SATRes SATChecker::solve(const IntVector* assum)
{
  SATRes res = SAT_NoRes;
  _start_sat_timer();
  if (_pre_mode && _psolver->is_preprocessing()) {
      res = _psolver->preprocess(_pre_mode == 1); // _pre_mode == 1 means turn off
      if (_pre_mode == 1) { _pre_mode = 0; }
  }
  if (res == SAT_NoRes)
    res = (assum == nullptr) ? _psolver->solve() : _psolver->solve(*assum);
  _stop_sat_timer(res);
  _sat_calls++;
  return res;
}


/* In the case the last SAT call returned UNSAT, this method will get the core
 * from the SAT solver, and will add the GIDs of unnecessary groups (i.e. those
 * none of whose clauses are in the core) into unnec_gids; no locking is done
 * in this method, however it reads from md.
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
      if ((*pgid != 0) && !md.r(*pgid) && !md.nec(*pgid) 
          && (gcore.find(*pgid) == gcore.end()))
        unnec_gids.insert(*pgid);
    }
    DBG(cout << "=SATChecker::refine() - core is clean, refined." << endl;);
  } else {
    DBG(cout << "=SATChecker::refine() - core is tainted, not refined." << endl;);
  }
  // done
}


// TODO: what's the best way to do this ? Again lots of repeated code ...

/* The variable-based version of sync_solver()
 */     
void SATChecker::vsync_solver(const MUSData& md)
{
  // TEMP: get rid of const - TODO: fix when BasicGroupSet is const-correct
  // Actually, now that we're modifying groupset, it should not be const in the 
  // first place ! Figure this out !!!
  BasicGroupSet& gs(*const_cast<BasicGroupSet*>(&md.gset()));

  // case (a): SAT solver empty
  if (_psolver->gsize() == 0) {
    // load all clauses; note that clauses composed solvely of group 0 variables
    // are made final right away
    for (cvec_iterator pcl = gs.begin(); pcl != gs.end(); ++pcl) {
      BasicClause* cl = *pcl;
      if (cl->removed()) // weird
        continue;
      // count the number of g0 literals
      for (Literator plit = cl->abegin(); plit != cl->aend(); ++plit) {
        if (gs.get_var_grp_id(abs(*plit)) == 0)
          ++cl->g0v_count();
      }
      if (cl->g0v_count() == cl->asize())
        _psolver->add_final_clause(cl);
      else
        _psolver->add_clause(cl);
    }
    // remove removed groups
    for (GIDListCIterator pg = md.r_list().begin(); pg != md.r_list().end(); ++pg)
      del_vgroup(_psolver, gs, *pg);
    for (GIDListCIterator pg = md.f_list().begin(); pg != md.f_list().end(); ++pg)
      make_vgroup_final(_psolver, gs, *pg);
  }
  // case (b): assume that some groups are gone from gset
  else {
    // scan the list from the front and for each group remove it from the
    // solver -- if a group is not in the solver, stop
    // NOTE: no early break here, because one group may have been removed
    // because of some other group -- instead, main thread has to empty the lists
    for (GIDListCIterator pg = md.r_list().begin(); pg != md.r_list().end(); ++pg)
      del_vgroup(_psolver, gs, *pg);
    // same for final clauses
    for (GIDListCIterator pg = md.f_list().begin(); pg != md.f_list().end(); ++pg)
      make_vgroup_final(_psolver, gs, *pg);
  }
}

/* Variable-based version of refine()
 * 
 * TODO: this seems to take quite a lot of time e.g. 13sec for 70sec of SAT solving
 * time on UTI-20-5t0.cnf.gz with VMR (nor EVMR); perpahs there's a better way
 * to do this.
 */
void SATChecker::vrefine(const MUSData& md, GIDSet& unnec_gids, GIDSet& ft_gids, GID rr_gid)
{
  // TEMP: get rid of const - TODO: fix when BasicGroupSet is const-correct
  BasicGroupSet& gs(*const_cast<BasicGroupSet*>(&md.gset()));

  // put all unremoved and not known to be necessary groups that are not in the
  // group core into unnec_gids
  GIDSet& gcore = _psolver->get_group_unsat_core();
  NDBG(cout << "SATChecker::vrefine(): gcore " << gcore << endl;);
  // compute variable core -- i.e. set of variable groups that appear in the
  // clauses of the core;
  GIDSet vgcore;
  bool is_clean = true;
  for (GIDSet::iterator pgid = gcore.begin(); pgid != gcore.end(); ++pgid) {
    if (*pgid == rr_gid) {
      is_clean = false;
      continue;
    }
    BasicClauseVector& gcl = gs.gclauses(*pgid);
    assert(gcl.size() == 1);
    BasicClause* cl = *gcl.begin();
    assert(!cl->removed());
    for (CLiterator plit = cl->abegin(); plit != cl->aend(); ++plit)
      vgcore.insert(gs.get_var_grp_id(abs(*plit)));
  }
  DBG(cout << "SATChecker::vrefine(): vgcore " << vgcore << endl;);
  DBG(cout << "SATChecker::vrefine(): core is " 
      << ((is_clean) ? "clean" : "tainted") << endl;);
  // if the core is clean, the refinment is safe and we add variables outside the
  // core into unnec_gids; otherwise, just fast-track them
  for (vgset_iterator pvgid = gs.vgbegin(); pvgid != gs.vgend(); ++pvgid) {
    if ((*pvgid != 0) && !md.r(*pvgid) && !md.nec(*pvgid)) {
      if (vgcore.find(*pvgid) == vgcore.end())
        (is_clean ? unnec_gids : ft_gids).insert(*pvgid);
    }
  }
  // note that the invariant of vgcore.size() + unnec_gids.size() == remaining 
  // group count does not hold anymore, because there might be necessary, but not
  // finalized groups (i.e. they will appear in the core, even though they are
  // necessary -- this couldn't happen before because necessary groups were 
  // immediately finalized; I could probably remember the number of finalized
  // variables ...

  // done
}

//
// ------------------------  Local implementations  ----------------------------
//

namespace {

  // Helpers for the variable based-methods; 
  // 
  // TODO: lots of repeated code; figure out a better way to do it ...
  //
  // TODO2: all these calls are expensive if the occlists are not cleaned
  // up as we go along ... but then its kind of breaks the design. 

  // If A::act(GID) returns 1 or -1 -- return immediately, 0 -- do nothing
  template <class A>
  int do_occslist(OccsList& occs, LINT lit, A& act) 
  {
    BasicClauseList& cp = occs.clauses(lit);
    for (BasicClauseList::iterator pcl = cp.begin(); pcl != cp.end(); ) {
      if ((*pcl)->removed()) { 
        pcl = cp.erase(pcl); continue; 
      }
      int rval = act.process((*pcl)->get_grp_id());
      if (rval)
        return rval;
      ++pcl;
    }
    return 0;
  }
  template <class A>
  bool process_vgroup(BasicGroupSet& gs, GID vgid, A act) 
  {
    const VarVector& vars = gs.vgvars(vgid);
    OccsList& occs = gs.occs_list();
    for (VarVector::const_iterator pvar = vars.begin(); pvar != vars.end(); ++pvar) {
      if ((do_occslist(occs, *pvar, act) == 1) || 
          (do_occslist(occs, -*pvar, act) == 1))
        return true;
    }
    return false;
  }

  /* Returns true if a variable group exists in the solver -- that is there's
   * at least one clause in the solver that has a variable from the group.
   */
  struct ExistChecker {
    SATSolverWrapper* _psolver;
    ExistChecker(SATSolverWrapper* psolver): _psolver(psolver) {}
    int process(GID cgid) { return _psolver->exists_group(cgid); }
  };
  bool exists_vgroup(SATSolverWrapper* psolver, BasicGroupSet& gs, GID vgid)
  {
    DBG(cout << "+exists_vgroup(" << vgid << ")" << endl;);
    bool rval = process_vgroup(gs, vgid, ExistChecker(psolver));
    DBG(cout << "-exists_vgroup(" << vgid << ") = " << rval << endl;);
    return rval;
  }

  /* Returns true if a variable group is final -- i.e. at least one final clause
   * in the solver with this variable
   */
  struct FinalChecker {
    SATSolverWrapper* _psolver;
    FinalChecker(SATSolverWrapper* psolver): _psolver(psolver) {}
    int process(GID cgid) { 
      return _psolver->exists_group(cgid) && _psolver->is_group_final(cgid); 
    }
  };
  bool is_vgroup_final(SATSolverWrapper* psolver, BasicGroupSet& gs, GID vgid)
  {
    return process_vgroup(gs, vgid, FinalChecker(psolver));
  }

  /* Deletes variable group from the solver -- that is, all clauses that have one 
   * or more variables from vgid, and have not been already removed, are permanently 
   * removed from the solver.
   */
  struct Deleter {
    SATSolverWrapper* _psolver;
    Deleter(SATSolverWrapper* psolver): _psolver(psolver) {}
    int process(GID cgid) { 
      if (_psolver->exists_group(cgid))
        _psolver->del_group(cgid);
      return 0;
    }
  };
  void del_vgroup(SATSolverWrapper* psolver, BasicGroupSet& gs, GID vgid)
  {
    DBG(cout << "=del_vgroup(" << vgid << ")" << endl;);
    process_vgroup(gs, vgid, Deleter(psolver));
    // now that the clauses have been actually removed from the solver, we can
    // clean up the occlists
    VarVector& vars = gs.vgvars(vgid);
    OccsList& occs = gs.occs_list();
    for (VarVector::const_iterator pvar = vars.begin(); pvar != vars.end(); ++pvar) {
      BasicClauseList& cp = occs.clauses(*pvar);
      for (BasicClauseList::iterator pcl = cp.begin(); pcl != cp.end(); ) {
        if (!(*pcl)->removed()) {
          (*pcl)->mark_removed();
          if (gs.has_occs_list())
            gs.occs_list().update_active_sizes(*pcl);
        }
        pcl = cp.erase(pcl);
      }
      BasicClauseList& cn = occs.clauses(-*pvar);
      for (BasicClauseList::iterator pcl = cn.begin(); pcl != cn.end(); ) {
        if (!(*pcl)->removed()) {
          (*pcl)->mark_removed();
          if (gs.has_occs_list())
            gs.occs_list().update_active_sizes(*pcl);
        }
        pcl = cp.erase(pcl);
      }
    }
  }

  /* Finalizes variable group in the solver -- note that just becasue a clause
   * has a necessary variable does not mean that the clause itself is necessary.
   * As such clauses cannot be finalized until all of their variables are known
   * to be necessary. Hence the Finalizer updates the count for the clause, and
   * if it reaches its length, it finalizes it.
   */
  struct Finalizer {
    SATSolverWrapper* _psolver;
    BasicGroupSet& _gs;
    Finalizer(SATSolverWrapper* psolver, BasicGroupSet& gs) 
      : _psolver(psolver), _gs(gs) {}
    int process(GID cgid) { 
      BasicClause* cl = *_gs.gclauses(cgid).begin();
      if ((++cl->nv_count()) + cl->g0v_count() == cl->asize()) {
        if (_psolver->exists_group(cgid) && !_psolver->is_group_final(cgid))
          _psolver->make_group_final(cgid);
      }
      return 0;
    }
  };
  void make_vgroup_final(SATSolverWrapper* psolver, BasicGroupSet& gs, GID vgid)
  {
    process_vgroup(gs, vgid, Finalizer(psolver, gs));
  }

  // CAUTION: activate/deactivate operations are not exact inverse of each other,
  // because they have to rely on the status of clauses in the solver (unless it
  // becomes an issue), in particular deactivate(v1),deactivate(v2),activate(v2) 
  // will not have the same as deactivate(v1) because a clause that has both
  // v1 and v2 will be active in the first case, and inactive in the second.
  // So, the point is that deactivate(v) has to be followed by activate(v)

  /* Deactivates variable group in the solver -- that is, all clauses that have one 
   * or more variables from vgid, and are active, not removed, and not finalized,
   * are deactivated.
   */
  struct Deactivator {
    SATSolverWrapper* _psolver;
    Deactivator(SATSolverWrapper* psolver): _psolver(psolver) {}
    int process(GID cgid) { 
      if (_psolver->exists_group(cgid) && 
          !_psolver->is_group_final(cgid) && 
          _psolver->is_group_active(cgid))
        _psolver->deactivate_group(cgid);
      return 0;
    }
  };
  void deactivate_vgroup(SATSolverWrapper* psolver, BasicGroupSet& gs, GID vgid)
  {
    process_vgroup(gs, vgid, Deactivator(psolver));
  }

  /* Activates variable group in the solver -- that is, all clauses that have one 
   * or more variables from vgid, and are deactivated, not removed, and not finalized,
   * are activated.
   */
  struct Activator {
    SATSolverWrapper* _psolver;
    Activator(SATSolverWrapper* psolver): _psolver(psolver) {}
    int process(GID cgid) { 
      if (_psolver->exists_group(cgid) && 
          !_psolver->is_group_final(cgid) && 
          !_psolver->is_group_active(cgid))
        _psolver->activate_group(cgid);
      return 0;
    }
  };
  void activate_vgroup(SATSolverWrapper* psolver, BasicGroupSet& gs, GID vgid)
  {
    process_vgroup(gs, vgid, Activator(psolver));
  }

  /* Collects a list of clauses in the VGID.
   */
  struct Collector {
    BasicGroupSet& _gs;
    BasicClauseVector& _clvec;
    Collector(BasicGroupSet& gs, BasicClauseVector& clvec): _gs(gs), _clvec(clvec) {}
    int process(GID cgid) { 
      assert(_gs.gclauses(cgid).size() == 1);
      BasicClause* cl = *_gs.gclauses(cgid).begin();
      _clvec.push_back(cl);
      return 0;
    }
  };
  void collect_clauses(BasicGroupSet& gs, GID vgid, BasicClauseVector& clvec)
  {
    process_vgroup(gs, vgid, Collector(gs, clvec));
  }

}

