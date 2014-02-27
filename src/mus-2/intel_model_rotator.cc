/*----------------------------------------------------------------------------*\
 * File:        intel_model_rotator.cc
 *
 * Description: This is an experimental model rotator that assumes a certain
 *              structure in the groups and the group 0. Targeted to Intel
 *              A/R instances, and Huan's decomposition instances.
 *
 * Author:      antonb
 * 
 * Notes:
 *
 *
 *                                              Copyright (c) 2012, Anton Belov
 \*----------------------------------------------------------------------------*/

#include <cassert>
#include <cstdint>
#include <ext/hash_set>
#include <iostream>
#include <list>
#include <queue>
#include <unordered_set>
#include "basic_group_set.hh"
#include "types.hh"
#include "model_rotator.hh"
#include "utils.hh"

using namespace std;
using namespace __gnu_cxx;

//#define DBG(x) x
//#define CHK(x) x

namespace {

  /* This is an entry into the rotation queue. */
  struct rot_queue_entry {
    GID gid = 0;                        // the falsified GID
    BasicClauseVector fclauses;         // the falsified clauses; inv: the gid of 
                                        // all clauses is gid
    vector<ULINT> delta;                // the delta from the original assignment 
                                        // inv: gid is the only group falsified
                                        // by initial assignment + delta
    rot_queue_entry(void) {}
    rot_queue_entry(GID g) : gid(g) {}
    rot_queue_entry(GID g, vector<ULINT>& d) : gid(g), delta(d) {}
  };

  /* False clause container for SLS (keeps unit and binary clauses separately)
   */
  class FalseClauseContainer { 
  public:
    FalseClauseContainer(void) { _2clauses.reserve(1000); _clauses.reserve(1000); }
    // add a clause
    void add_clause(BasicClause* cl) { 
      (cl->asize() <= 2 ? _2clauses : _clauses).push_back(cl); 
    }
    // removes all clauses with the specified literal
    void remove_clauses_with_lit(LINT lit) {
      _remove_clauses_with_lit(_2clauses, lit);
      _remove_clauses_with_lit(_clauses, lit);
    }
    // picks a random clause, preferring short
    BasicClause* pick_short_clause(void) {
      assert(size() && "container shouldn't be empty");
      return _pick_clause(_2clauses.size() ? _2clauses : _clauses);
    }
    // size
    unsigned size(void) { return _2clauses.size() + _clauses.size(); }
    // clears everything
    void clear(void) { _2clauses.clear(); _clauses.clear(); } 
    // debug dump
    void dump(ostream& out = cout) { 
      for (BasicClause* cl : _2clauses) { cl->dump(out); out << " "; }
      for (BasicClause* cl : _clauses) { cl->dump(out); out << " "; }
    }
  private:
    BasicClauseVector _2clauses;        // short clauses
    BasicClauseVector _clauses;         // the rest
    // removes clauses with literal from the container
    void _remove_clauses_with_lit(BasicClauseVector& clv, LINT lit) {
      unsigned size = clv.size();       
      if (size) {
        BasicClause** data = clv.data();
        ULINT var = abs(lit);
        for (unsigned i = 0; i < size; ) {
          BasicClause* cl = data[i];
          CLiterator plit = cl->afind(var);
          if ((plit != cl->aend()) && (*plit == lit)) {
            if (i < size - 1) // not last - swap
              data[i] = data[size-1];
            size--;
          } else {
            i++;
          }
        }
        clv.resize(size);
      }
    }
    // picks a random clause from the container
    BasicClause* _pick_clause(BasicClauseVector& clv) {
      return (clv.empty()) ? 0 : clv[Utils::random_int(clv.size()-1)]; 
    }
  };


  /* This is debugging routine that dumps current formula and the assignment
   * into a file */
  void dump_to_file(MUSData& md, 
                    const BasicClause* target_cl,
                    const IntVector& curr_ass);

  /* This is an expensive routine that checks a bunch of invariants of the
   * algorithm */
  template<class CC>
  void check_invariants(const BasicGroupSet& gs,
                        const IntVector& orig_model, 
                        const IntVector& curr_ass,
                        const vector<ULINT>& delta, 
                        const IntelModelRotator::VarSet& multiflip,
                        const CC& fclauses,
                        GID gid);

} // anonymous namespace

/* Handles the RotateModel work item.
 *
 * Current parameters: 
 *      config.param1() - max. number of points in conflict graph search
 *      config.param2() - if 1, use resolution graph
 * 
 */
bool IntelModelRotator::process(RotateModel& rm)
{
  MUSData& md = rm.md();
  BasicGroupSet& gs = md.gset();
  OccsList& o_list = gs.occs_list();
  const IntVector& orig_model = rm.model();

  // store/retrieve imporant parameters
  _max_points = config.get_param1();
  _abort_when_no_target = config.get_param2() & 1;
  _allow_revisit_nodes = config.get_param2() & 2;
  _use_rgraph = config.get_param2() & 4;
  switch (config.get_param3()) {
  case 0 : _fixer = Fixer::None; break;
  case 1 : _fixer = Fixer::SLS; 
    _sls_cutoff = config.get_param4(); _sls_tries = config.get_param5();
    break;
  case 2 : _fixer = Fixer::CDCL; 
    _cdcl_max_conf = config.get_param4();
    _transmit_model = config.get_param5();
    break;
  }
  _pmd = &md;
  _pgs = &gs;
  _po_list = &o_list;

  DBG(cout << "+IntelModelRotator::process(" << rm.gid() << ")" << endl;);
  if (config.get_verbosity() >= 2) {
    cout_pref << "intelmr:" 
              << " max_points=" << _max_points
              << ", abort_when_no_target=" << _abort_when_no_target
              << ", allow_revisit_nodes=" << _allow_revisit_nodes
              << ", use_rgraph=" << _use_rgraph;
    switch (config.get_param3()) {
    case 0 : cout << ", fixer=None" << endl; break;
    case 1 : cout << ", fixer=SLS, cutoff=" << _sls_cutoff 
                  << ", tries=" << _sls_tries << endl;
      break;
    case 2 : cout << ", fixer=CDCL, maxconf=" << _cdcl_max_conf
                  << ", transmit_model=" << _transmit_model
                  << endl;
      break;
    }
  }

  // models are coded in terms of their deltas to the original model - a delta
  // is a list or set of variables in the original model that should be flipped;
  // a work queue entry consists of a group, a set of clauses in that group,
  // and delta for it -- the invariant is that the set of clauses are exactly
  // the false clauses under the assignment represented by delta

  queue<rot_queue_entry> rot_queue;

  // the first entry (delta is empty)
  rot_queue.emplace(rm.gid());
  BasicClauseVector& fclauses = rot_queue.back().fclauses;
  for (auto& cl : gs.gclauses(rm.gid()))
    if (!cl->removed() && Utils::tv_clause(orig_model, cl) == -1)
      fclauses.push_back(cl);
  if (_fixer == Fixer::CDCL)
    _solver.make_group_final(rm.gid());
  DBG(cout << "  " << fclauses.size() << " initial falsified clauses" << endl;);

  // a copy of the original model -- will be used as working assignment.
  IntVector curr_ass(orig_model);
  // a set of target gids -- all untested group ids
  GIDSet target_gids;
  for_each(gs.gbegin(), gs.gend(), [&](GID gid) { 
      if (gid && (gid != rm.gid()) && md.untested(gid)) target_gids.insert(gid); 
    });
  // go ...
  while (!rot_queue.empty()) {
    rot_queue_entry& e = rot_queue.front();
    GID gid = e.gid;
    BasicClauseVector& fclauses = e.fclauses;
    VarSet multiflip;
    HashedClauseSet new_fclauses;
    GIDSet new_fgids;

    DBG(cout << "Rotating gid=" << gid << ", delta: "; PRINT_ELEMENTS(e.delta););

    // update working assignment
    Utils::multiflip(curr_ass, e.delta);

    DBG(cout << "falsified clauses: "; 
        for (auto cl : fclauses) { cl->dump(); cout << " "; } cout << endl;);
    CHK(check_invariants(gs, orig_model, curr_ass, e.delta, multiflip, fclauses, gid););

    // main loop over satisfying products
    Utils::ProductGenerator pg(fclauses);
  _inner:
    while (pg.has_next_product()) {
      multiflip.clear();
      new_fclauses.clear();
      new_fgids.clear();
      const vector<LINT>& product = pg.next_product();
      for (LINT lit : product) { multiflip.insert(abs(lit)); }
      Utils::multiflip(curr_ass, multiflip);
      // compute the newly falsified clauses (if gid is there, get out)
      for (ULINT var : multiflip) {
        for (BasicClause* cl : o_list.clauses((curr_ass[var] == 1) ? -var : var)) {
          if (cl->removed()) { continue; }
          if (Utils::tv_clause(curr_ass, cl) == -1) {
            if (cl->get_grp_id() == gid) { // get out
              Utils::multiflip(curr_ass, multiflip);
              goto _inner; 
            } 
            new_fclauses.insert(cl);
            new_fgids.insert(cl->get_grp_id());
          }
        }
      }
      DBG(cout << "got satisfying multiflip: "; PRINT_ELEMENTS(multiflip););
      assert(new_fgids.size()
             && "the set of groups falsified after the multiflip cannot be empty");
      CHK(check_invariants(gs, orig_model, curr_ass, e.delta, multiflip, new_fclauses, gid_Undef););

      GID new_gid = gid_Undef;
      GID target_gid = gid_Undef;
      _target_search_time -= RUSAGE::read_cpu_time();
      find_target(gid, target_gids, // in
                  new_fclauses, new_fgids, curr_ass, multiflip, // in-out
                  new_gid, target_gid); // out
      _target_search_time += RUSAGE::read_cpu_time();
      _targets_searched++;
      if (new_gid != gid_Undef) { 
        _targets_found_necc++; 
      } else if (target_gid != gid_Undef) { 
        _targets_found_unkn++; 
      }
      DBG(cout << "  finished looking for target: " 
          << "new_gid = " << (int)new_gid  << ", target_gid = " << (int)target_gid 
          << ", new_fgids = " << new_fgids << endl;);
      CHK(check_invariants(gs, orig_model, curr_ass, e.delta, multiflip, new_fclauses, gid_Undef););

      if ((new_gid == gid_Undef) && (target_gid != gid_Undef)) {
        // try to prove the target; if can't -- fastrack it
        bool fixed = false;
        VarSet delta;
        double time = -RUSAGE::read_cpu_time();
        if (_fixer == Fixer::SLS) {
          fixed = fix_assignment_sls(target_gid, _sls_cutoff, _sls_tries, _sls_noise, // in
                                     curr_ass, delta, new_fclauses); // in-out
        } else if (_fixer == Fixer::CDCL) {
          fixed = fix_assignment_cdcl(target_gid, _cdcl_max_conf,       // in
                                      curr_ass, delta, new_fclauses);   // in-out
        }
        time += RUSAGE::read_cpu_time();
        _target_prove_time += time;
        DBG(if (_fixer != Fixer::None) 
              cout << "  " << ((fixed) ? "fixed" : "couldn't fix") << " assignment, "
                "time = " << time << " sec." << endl;);
        if (fixed) {
          new_gid = target_gid;
          for (ULINT var : delta) { // merge
            auto pv = multiflip.find(var);
            if (pv == multiflip.end())
              multiflip.insert(var);
            else
              multiflip.erase(pv);
          }
          _targets_proved++;
          _proved_targets_time += time;
        } else if (rm.collect_ft_gids()) {
          rm.ft_gids().insert(target_gid);
        }
        // block the target regardless
        target_gids.erase(target_gid);
      }
      CHK(check_invariants(gs, orig_model, curr_ass, e.delta, multiflip, new_fclauses, new_gid););

      // if we have something - enqueue it
      if (new_gid != gid_Undef) {
        // here we have an option to pick into the globally known set of 
        // necessary gids, as well as the local one - we'll do both (not sure 
        // about it, actually)
        if (!md.nec(new_gid) && !rm.nec_gids().count(new_gid)) {
          rm.nec_gids().insert(new_gid);
          rot_queue.emplace(new_gid);
          copy(new_fclauses.begin(), new_fclauses.end(), back_inserter(rot_queue.back().fclauses));
          copy(e.delta.begin(), e.delta.end(), back_inserter(rot_queue.back().delta));
          copy(multiflip.begin(), multiflip.end(), back_inserter(rot_queue.back().delta));
          if (_fixer == Fixer::CDCL)
            _solver.make_group_final(new_gid); // right away ...
          DBG(cout << "  put " << new_gid << " on the queue" << endl;);
        } 
        DBG(else { cout << "  already known to be necessary, skipped." << endl; });
        target_gids.erase(new_gid);
      }
      Utils::multiflip(curr_ass, multiflip);
      if (_abort_when_no_target && (new_gid == gid_Undef) && (target_gid == gid_Undef))
        break; // give up
      DBG(if (pg.has_next_product()) cout << "  going for the next product" << endl;);
    } // loop over products
    
    // done with this one
    Utils::multiflip(curr_ass, e.delta);
    rot_queue.pop();
    _num_points++;
  } // queue
  rm.set_completed();
  DBG(cout << "-IntelModelRotator::process(" << rm.gid() << ")" << endl;);
  return rm.completed();
}


// Starting from the given container of falsified clauses, searches the conflict 
// (or resolution) graph for a path to some clause with a GID among the specified
// set of target_gids. If found, the target clause is returned, and the path  
// starting from the initial clause (in fclauses) is populated into in a vector 
// of variables 'path' (if not 0). The function returns 0 and the path vector 
// is empty if nothing is found. The search space size can be controlled by 
// passing in the max_points parameter (if > 0).
template<class C>
BasicClause* IntelModelRotator::analyze_graph(const C& fclauses,                // in 
                                              const GIDSet& target_gids,        // in
                                              bool new_search,                  // in
                                              vector<ULINT>* path)              // out
{
  assert((!path || path->empty()) && "out vector should be empty");
  OccsList& o_list = *_po_list;
  BasicClause* result = 0;
  if (path) { path->clear(); }

  static unsigned visited_gen = 0;
  if (new_search) { visited_gen++; }
    
  queue<BasicClause*> q;      // queue for doing BFS
  unsigned v_count = 0;            // count of visited points

  // initialize the search
  for (BasicClause* cl : fclauses) {
    NDBG(cout << "  initial clause: "; cl->dump(); cout << endl;);
    assert(!cl->removed()); 
    cl->set_visited_gen(visited_gen);
    cl->set_incoming_lit(0);
    cl->set_incoming_parent(0); // i.e. the "original"
    q.push(cl);
    v_count++;
  }
  // go exploring
  while (!q.empty()) {
    BasicClause* cl = q.front();
    NDBG(cout << "  clause: "; cl->dump(); cout << ", neighbours: " << endl;);
    for (auto plit = cl->abegin(); plit != cl->aend(); ++plit) {
      NDBG(cout << "    lit=" << *plit << ": ");
      BasicClauseList& l = o_list.clauses(-*plit);
      for (auto cl2 : l) {
        if (cl2->removed()) { continue; }
        // skip if tautology and doing resolution graph
        NDBG(cl2->dump(););
        if (_use_rgraph && Utils::taut_resolvent(cl, cl2, *plit)) { // might be expensive !
          NDBG(cout << " tautology, skipped";);
          continue;
        }
        if (cl2->visited_gen() < visited_gen) {
          cl2->set_visited_gen(visited_gen);
          cl2->set_incoming_lit(*plit);
          cl2->set_incoming_parent(cl);
          q.push(cl2);
          v_count++;
          // now, let's check if we got anywhere ...
          if (target_gids.find(cl2->get_grp_id()) != target_gids.end()) {
            DBG(cout << "  found target clause"; cl2->dump(); cout << " path: ");
            result = cl2;
            if (path) {
              for (BasicClause* c = cl2; c->incoming_parent() != 0; 
                   c = c->incoming_parent()) {
                DBG(cout << " from "; c->incoming_parent()->dump(); 
                    cout << " on " << c->incoming_lit() << ", ";);
                path->push_back(abs(c->incoming_lit()));
              }
            }
            NDBG(cout << endl << "  target gid: " << cl2->get_grp_id() << endl;);
            goto _done;
          }
          if ((_max_points > 0) && (v_count > _max_points))
            goto _done;
        }
      }
      NDBG(cout << endl;);
    }
    NDBG(cout << endl;);
    q.pop();
  }
 _done:
  DBG(cout << "  visited nodes = " << v_count << endl;);
  return result;
}


// Looks for either a new necessary group ID or a new target group ID
void IntelModelRotator::find_target(GID source_gid,                     // in
                                    GIDSet& target_gids,                // in-out
                                    HashedClauseSet& new_fclauses,      // in-out
                                    GIDSet& new_fgids,                  // in-out
                                    IntVector& curr_ass,                // in-out
                                    VarSet& multiflip,                  // in-out
                                    GID& new_gid,                       // out
                                    GID& target_gid)                    // out
{
  BasicGroupSet& gs = *_pgs;
  OccsList& o_list = *_po_list;
  static bool new_search = true; // will become false on the first search
  if (_allow_revisit_nodes)
    new_search = true;
  for (int attempts = 0; attempts < 10; attempts++) {
    if ((new_fgids.size() == 1) && (*new_fgids.begin() != 0)) { // perfect !
      new_gid = *new_fgids.begin();
      if (target_gids.count(new_gid)) {
        DBG(cout << "  found new necessary group " << new_gid << " cheaply." << endl;);
        break;
      }
      new_gid = gid_Undef;
      break; // here there's an option to continue to look for new target
    } 
    if (new_fgids.size() > 1) {
      // if target is already known (this will happen if we analyzed the graph
      // pick it, otherwise go for a new one
      if ((target_gid == gid_Undef) || !new_fgids.count(target_gid)) {
        auto pg = find_if(new_fgids.begin(), new_fgids.end(),
                          [&](GID gid) { return target_gids.count(gid); });
        if (pg != new_fgids.end())    
          target_gid = *pg;
      }
      DBG(if (target_gid != gid_Undef) 
            cout << "  found new target group " << target_gid << endl;);
      break; // also, there's an option to continue
    } 
    // only g0 is falsified -- find a target in the graph
    if (target_gid != gid_Undef) {
      target_gids.erase(target_gid);
      target_gid = gid_Undef;
    }
    DBG(double time = RUSAGE::read_cpu_time(););
    BasicClause* target_cl = analyze_graph(new_fclauses, target_gids,
                                           new_search, 0);
    new_search = false;
    DBG(cout << "  finished graph analysis, time = " << 
        (RUSAGE::read_cpu_time() - time) << " sec." << endl;);
    if (!target_cl) // can't do anything, break out
      break;
    vector<ULINT> extra_flip; // need to falsify the target (but carefully)
    for_each(target_cl->abegin(), target_cl->aend(), [&](LINT lit) {
        if (Utils::tv_lit(curr_ass, lit) == -1)
          extra_flip.push_back(abs(lit)); 
      });
    Utils::multiflip(curr_ass, extra_flip);
    bool still_sat = true;
    for (BasicClause* cl : gs.gclauses(source_gid))
      if (Utils::tv_clause(curr_ass, cl) == -1) { // broke it, undo, and try again
        DBG(cout << "  extra flip falsifies the group, new attempt ..." << endl;);
        Utils::multiflip(curr_ass, extra_flip);
        still_sat = false; 
        break;
      }
    if (still_sat) {
      for (auto pcl = new_fclauses.begin(); pcl != new_fclauses.end(); ) {
        if (Utils::tv_clause(curr_ass, *pcl) == 1)
          pcl = new_fclauses.erase(pcl);    
        else
          ++pcl;
      }     
      for (ULINT var : extra_flip) {
        for (BasicClause* cl : o_list.clauses((curr_ass[var] == 1) ? -var : var)) {
          if (cl->removed()) { continue; }
          if (Utils::tv_clause(curr_ass, cl) == -1) {
            new_fclauses.insert(cl);
            new_fgids.insert(cl->get_grp_id());
          }
          multiflip.insert(var);
        }
      }
      DBG(cout << "  looks like a good target, re-checking." << endl;);
      target_gid = target_cl->get_grp_id();
    }
  } // looking for target
}


// This is main routine responsible for "fixing" the current assignment
// using SLS. On success the routine modifies curr_ass, delta, init_fclauses 
// and returns true. Otherwise in-out parameters remain unchanged.
template<class CLSET, class VSET>
bool IntelModelRotator::fix_assignment_sls(GID target_gid,                // in
                                           unsigned cutoff,               // in
                                           unsigned tries,                // in
                                           float noise,                   // in
                                           IntVector& curr_ass,           // in-out
                                           VSET& delta,                   // in-out
                                           CLSET& init_fclauses)          // in-out
{
  assert(delta.empty() && "in-out delta should be empty");
  FalseClauseContainer fclauses;
  BasicGroupSet& gs = *_pgs;
  OccsList& o_list = *_po_list;
  unsigned total_steps = 0;
  bool found = false;
  VarSet frozen_vars;
    
  NDBG(cout << "+fix_assignment(), target group ID " << target_gid 
       << ", initial fclauses size =" << init_fclauses.size() << endl;);

  // freeze variables
  for (BasicClause* cl : init_fclauses)
    if (cl->get_grp_id() == target_gid)
      for_each(cl->abegin(), cl->aend(), [&](LINT lit) { frozen_vars.insert(abs(lit)); });

  // initialize tl counts
  for (BasicClause* cl : gs)
    if (!cl->removed() && cl->get_grp_id() != target_gid)
      cl->set_tl_count(Utils::num_tl_clause(curr_ass, cl));

  // outer SLS loop
  while (tries-- > 0) {
    fclauses.clear();
    for (BasicClause* cl : init_fclauses)
      if (cl->get_grp_id() != target_gid)
        fclauses.add_clause(cl);
    // undo flips, if any (need to update counts)
    for (ULINT var : delta) {
      Utils::flip(curr_ass, var);
      BasicClauseList& inc = o_list.clauses((curr_ass[var] == 1) ? var : -var);
      for (BasicClause* cl : inc) {
        if (cl->removed()) { continue; }
        if (cl->get_grp_id() != target_gid)
          cl->inc_tl_count();
      }
      BasicClauseList& dec = o_list.clauses((curr_ass[var] == 1) ? -var : var);
      for (BasicClause* cl : dec) {
        if (cl->removed()) { continue; }
        if (cl->get_grp_id() != target_gid)
          cl->dec_tl_count();
      }
    }
    delta.clear();
    NDBG(cout << "  new try; num.fclauses = " << fclauses.size() << endl;);
    NDBG(cout << "  false clauses: "; fclauses.dump(); cout << endl;);
    // inner SLS loop
    unsigned step = 0;
    bool abort = false;
    while (!abort && (cutoff > step++) && fclauses.size()) {
      NDBG(cout << "    new step; num.fclauses = " << fclauses.size() << endl;);
      NDBG(cout << "    false clauses: "; fclauses.dump(); cout << endl;);
      BasicClause* cand_cl = fclauses.pick_short_clause();
      vector<LINT> cand_lits;
      LINT flip_lit = 0;
      unsigned best_score = INT32_MAX; // the best-break value (smaller is better)
      for_each(cand_cl->abegin(), cand_cl->aend(), [&](LINT lit) {
          // the break-value of the is the number of uniquely satisfied clauses
          // with -lit (because they become unsat); ignoring target_gid and frozen
          // variables
          if (!frozen_vars.count(abs(lit))) {
            unsigned score = 0;
            BasicClauseList& lcl = o_list.clauses(-lit);
            for (auto pcl = lcl.begin(); pcl != lcl.end(); ) {
              if ((*pcl)->removed()) { pcl = lcl.erase(pcl); continue; }
              if (((*pcl)->get_grp_id() != target_gid) && ((*pcl)->tl_count() == 1)) {
                score++;
                if (score > best_score) { break; } 
              }
              ++pcl;
            }
            if (score < best_score) {
              cand_lits.clear();
              best_score = score;
            }
            if (score == best_score)
              cand_lits.push_back(lit);
          }
        });

      // if the best step is a worsening step, then with probability noise
      // randomly choose the literal to flip; otherwise pick one from the
      // candidate list
      if (best_score > 0) {
        if (Utils::random_double() < noise) {
          NDBG(cout << "    random walk" << endl;);
          cand_lits.clear();
          for_each(cand_cl->abegin(), cand_cl->aend(), [&](LINT lit) {
              if (!frozen_vars.count(abs(lit))) cand_lits.push_back(lit); });
        }
      }

      unsigned num_cand_lits = cand_lits.size();
      if (num_cand_lits == 0) {
        // this means we found falsified clause that cannot be satisfied
        DBG(cout << "    found un-satisfiable clause: "; cand_cl->dump(); 
            cout << endl;);
        abort = true;
        break;
      }
      NDBG(cout << "    best score " << best_score << ", num.cands "  
           << num_cand_lits << endl;);
      assert(num_cand_lits && "candidate list should not be empty");

      flip_lit = cand_lits[(num_cand_lits == 1) ? 0 : Utils::random_int(num_cand_lits-1)];
        
      // flip and update fclauses
      ULINT flip_var = abs(flip_lit);
      Utils::flip(curr_ass, flip_var);
      auto pd = delta.find(flip_var);
      if (pd == delta.end()) 
        delta.insert(flip_var);
      else
        delta.erase(pd);
      NDBG(cout << "    flipped " << abs(flip_lit) 
           << ", now " << curr_ass[abs(flip_lit)] << endl;);
      fclauses.remove_clauses_with_lit(flip_lit);
      NDBG(cout << "    removed true clauses, now: "; fclauses.dump(); cout << endl;);
      for (BasicClause* cl : o_list.clauses(-flip_lit)) {
        if (cl->removed()) { continue; }
        if ((cl->get_grp_id() != target_gid) && (cl->dec_tl_count() == 0)) {
          fclauses.add_clause(cl);
          NDBG(cout << "    added false clause "; cl->dump(); cout << endl);
        }
      }
      BasicClauseList& lcl = o_list.clauses(flip_lit);
      for (auto pcl = lcl.begin(); pcl != lcl.end(); ) {
        if ((*pcl)->removed()) { pcl = lcl.erase(pcl); continue; }
        if ((*pcl)->get_grp_id() != target_gid)
          (*pcl)->inc_tl_count();
        ++pcl;
      }
      total_steps++;
      //if (fclauses.size() > 100) // strayed too far
      //  break;
    } // inner loop
    if (fclauses.size() == 0) {
      found = true;
      break;
    }
  } // outer loop
  if (found) {
    init_fclauses.clear();
    for (BasicClause* cl : gs.gclauses(target_gid))
      if (Utils::tv_clause(curr_ass, cl) == -1)
        init_fclauses.insert(cl);
  } else {
    Utils::multiflip(curr_ass, delta);
    delta.clear();
  }
  DBG(cout << "  " << (found ? "found" : "couldn't find") << 
      " satisfying assignment in " << total_steps << " steps." << endl;);

  //if (!found)
  //  dump_to_file(md, target_cl, curr_ass);
     
  return found;
}


// This is main routine responsible for "fixing" the current assignment using
// CDCL. This is main routine responsible for "fixing" the current assignment
// using SLS. On success the routine modifies curr_ass, delta, init_fclauses 
// and returns true. Otherwise in-out parameters remain unchanged. The routine
// uses the underlying SAT solver.
template<class CLSET, class VSET>
bool IntelModelRotator::fix_assignment_cdcl(GID target_gid,           // in
                                            unsigned max_conf,        // in
                                            IntVector& curr_ass,      // in-out
                                            VSET& delta,              // in-out
                                            CLSET& init_fclauses)     // in-out
{
  assert(delta.empty() && "in-out delta should be empty");
  BasicGroupSet& gs = *_pgs;
  bool found = false;
  IntVector frozen_lits;
    
  DBG(cout << "+fix_assignment_cdcl(), target group ID " << target_gid
      << ", initial fclauses size =" << init_fclauses.size() << endl;);

  for (BasicClause* cl : init_fclauses)
    if (cl->get_grp_id() == target_gid)
      for_each(cl->abegin(), cl->aend(), [&](LINT lit) {
          if (find(frozen_lits.begin(), frozen_lits.end(), -lit) == frozen_lits.end())
            frozen_lits.push_back(-lit); 
        });
  if (!_solver.exists_group(target_gid) || !_solver.is_group_active(target_gid)) {
    assert(false && "target group should exist and be active");
    return false;
  }
  _solver.deactivate_group(target_gid);
  if (_transmit_model) { // "transmit" current model
    for (unsigned var = 1; var < curr_ass.size(); var++)
      if (curr_ass[var])
        _solver.set_phase(var, curr_ass[var] == 1);
  }
  _solver.set_max_conflicts(max_conf);
  _solver.init_run();
  DBG(cout << "  running SAT solver ... " << flush;);
  SATRes outcome = _solver.solve(frozen_lits);
  if (outcome == SAT_True) {
    DBG(cout << "SAT, reading in model." << endl;);
    // do the updates 
    IntVector& model = _solver.get_model();
    for (unsigned var = 1; var < curr_ass.size(); var++) {
      if (model[var] && (model[var] != curr_ass[var])) {
        Utils::flip(curr_ass, var);
        delta.insert(var);
      }
    }
    init_fclauses.clear();
    for (BasicClause* cl : gs.gclauses(target_gid)) {
      if (cl->removed()) { continue; }
      if (Utils::tv_clause(curr_ass, cl) == -1)
        init_fclauses.insert(cl);
    }
    found = true;
  } else {
    DBG(cout << ((outcome == SAT_False) ? "UNSAT" : "UNKNOWN") << ", giving up" << endl;);
  }
  _solver.activate_group(target_gid);
  _solver.reset_run();
  _solver.set_max_conflicts(-1);
       
  return found;
}



//
// ------------------------  Local implementations  ----------------------------
//

namespace {

  
  
  // This is debugging routine that dumps current formula and the assignment
  // into a file
  void dump_to_file(MUSData& md, 
                    const BasicClause* target_cl,
                    const IntVector& curr_ass)
  {
    BasicGroupSet& gs = md.gset();
    static int call_count = 0;
    call_count++;

    GID target_gid = target_cl->get_grp_id();
    set<LINT> false_lits;
    set<LINT> true_lits;
    for_each(target_cl->abegin(), target_cl->aend(),
             [&](LINT lit) { false_lits.insert(lit); true_lits.insert(-lit); });

    char name[100];

    sprintf(name, "g%d.cnf", call_count);
    ofstream fout(name);
    if (!fout)
      throw runtime_error("cannot open file");
    unsigned cl_count = 0;
    for (auto cl : gs) {
      if (cl->removed()) { continue; }
      if (cl->get_grp_id() == target_gid) { continue; }
      if (any_of(cl->abegin(), cl->aend(), 
                 [&](LINT lit) { return true_lits.count(lit); })) { continue; }
      for_each(cl->abegin(), cl->aend(), 
               [&](LINT lit) { if (!false_lits.count(lit)) fout << lit << " "; });
      fout << "0" << endl;
      cl_count++;
    }
    fout << "p cnf " << gs.max_var() << " " << cl_count << endl;
    fout.close();

    sprintf(name, "g%d.ass", call_count);
    ofstream fout2(name);
    if (!fout2)
      throw runtime_error("cannot open file");
    for (int var = 1; var < (int)curr_ass.size(); var++)
      if (curr_ass[var] && (target_cl->afind(var) == target_cl->aend()))
        fout2 << ((curr_ass[var] > 0) ? var : -var) << " " << endl;
    fout2 << endl;
    fout2.close();
    DBG(cout << "wrote out g0 and the model." << endl;);
    if (call_count == 10)
      exit(0);
  }

  /* This is an expensive routine that checks a bunch of invariants of the
   * algorithm, namely:
   *    - model consistency: curr_ass == orig_model + delta + multiflip
   *    - whether the clauses falsified by the curr_model are excatly those
   *      in the given clause container
   *    - whether all falsified clauses has GID gid (if not gid_Undef)
   */
  template<class CC>
  void check_invariants(const BasicGroupSet& gs,
                        const IntVector& orig_model, 
                        const IntVector& curr_ass,
                        const vector<ULINT>& delta, 
                        const IntelModelRotator::VarSet& multiflip,
                        const CC& fclauses,
                        GID gid)
  {
    bool passed = true;
    cout << "WARNING: expensive check ";
    IntVector tm(orig_model);   
    Utils::multiflip(tm, delta);
    Utils::multiflip(tm, multiflip);
    for (unsigned var = 1; var < tm.size(); var++) {
      if (tm[var] != curr_ass[var]) {
        cout << "error: curr_model[" << var << "]=" << curr_ass[var] 
             << ", but should be " << tm[var] << " ";
        passed = false;
      }
    }
    if (!passed) { goto _failed; }

    for (BasicClause* cl : fclauses) {
      if (cl->removed()) { 
        cout << "warning: "; cl->dump(); cout << " is removed ";
        continue; 
      }
      if (Utils::tv_clause(curr_ass, cl) != -1) {
        cout << "error: clause "; cl->dump(); cout << " should be false ";
        passed = false;
      }
    }
    if (!passed) { goto _failed; }

    if (gid != gid_Undef) {
      for (BasicClause* cl : fclauses) {
        if (cl->removed()) { continue; }
        if (cl->get_grp_id() != gid) {
          cout << "error: clause "; cl->dump(); 
          cout << " is false but has wrong GID ";
          passed = false;
        }
      }
    }
    if (!passed) goto _failed;
    
    for (BasicClause* cl : gs) {
      if (cl->removed()) { continue; }
      if (Utils::tv_clause(curr_ass, cl) == -1) {
        auto pc = find(fclauses.begin(), fclauses.end(), cl);
        if (pc == fclauses.end()) {
          cout << "error: clause "; cl->dump(); cout << " should not be false ";
          passed = false;
        }
      }
    }

    cout << "passed." << endl;
    return;
  _failed:
    cout << "FAILED" << endl;
    exit(0);
  }

} // anonymous namespace
