/*----------------------------------------------------------------------------*\
 * File:        intel_model_rotator2.cc
 *
 * Description: This is not a rotator, actually, its an approximator.
 *
 * Author:      antonb
 * 
 * Notes:
 *
 *
 *                                              Copyright (c) 2012, Anton Belov
 \*----------------------------------------------------------------------------*/

#include <cassert>
#include <ext/hash_set>
#include <iostream>
#include <list>
#include <queue>
#include "basic_group_set.hh"
#include "types.hh"
#include "model_rotator.hh"
#include "utils.hh"

using namespace std;
using namespace __gnu_cxx;

//#define DBG(x) x

namespace {

  // starting from the clauses of gid, look around; return the newly-found GID
  unsigned visited_gen = 0;
  GID analyze_graph(MUSData& md, GID init_gid, IntVector& model, 
                    int max_points, bool rgraph);

} // anonymous namespace

/* Handles the RotateModel work item
 */
bool IntelModelRotator2::process(RotateModel& rm)
{
  MUSData& md = rm.md();
  IntVector curr_ass(rm.model());
  int iters = config.get_param1();      
  if (iters <= 0) { iters = 1; }
  int max_points = config.get_param2();
  
  DBG(cout << "+IntelModelRotator2::process(" << rm.gid() << ")" << endl;);
  DBG(cout << " iterations=" << iters << "; max points=" << max_points << endl;);
  GID gid = rm.gid();
  visited_gen++; // not needed if not preventing re-visiting
  while ((--iters >= 0) && md.num_untested()) {
    DBG(double time = RUSAGE::read_cpu_time(););
    if (config.get_param3()) // prevent re-visiting among iterations
      visited_gen--; 
    GID new_gid = analyze_graph(md, gid, curr_ass, max_points, false);
    DBG(cout << "  finished analysis, new_gid = " << new_gid 
        << ", time = " << (RUSAGE::read_cpu_time() - time) << " sec." << endl;);
    if (new_gid == gid_Undef)
      break;
    // mark as *potentially* necessary ...
    md.pot_nec_gids().insert(new_gid);
    md.mark_necessary(new_gid);
    gid = new_gid;
  }
  rm.set_completed();
  DBG(cout << "-IntelModelRotator2::process(" << rm.gid() << ")" << endl;);
  return rm.completed();
}


//
// ------------------------  Local implementations  ----------------------------
//

namespace {

  // Starting from the clauses of start_gid falsified by the assignment model,
  // look around the conflict (or resolution) graph, until a new GID is encountered.
  // When new GID is found, the model is modified so that this new GID is falsified,
  // and the new GID is returned. gid_Undef is returned if nothing is found.
  GID analyze_graph(MUSData& md, GID start_gid, IntVector& model, 
                    int max_points, bool rgraph)
  {
    BasicGroupSet& gs = md.gset();
    OccsList& o_list = gs.occs_list();
    GID result = gid_Undef;
    visited_gen++;
    
    // queue for doing BFS
    queue<BasicClause*> q;
    int v_count = 0;
    // populate the queue with the currently falsified clauses
    // collect the set of variables that appear in the falsified clauses of gid
    const BasicClauseVector& gclauses = gs.gclauses(start_gid);
    for (auto& cl : gclauses) { 
      assert(!cl->removed()); 
      if (Utils::tv_clause(model, cl) == -1) {
        cl->set_visited_gen(visited_gen);
        cl->set_incoming_lit(0);
        cl->set_incoming_parent(0); // i.e. the "original"
        q.push(cl); 
        v_count++;
      }
    }
    // go exploring
    while (!q.empty()) {
      BasicClause* cl = q.front();
      NDBG(cout << "  clause: "; cl->dump(); 
           cout << " (tl=" << Utils::num_tl_clause(model, cl) << "), neighbours: " << endl;);
      for (auto plit = cl->abegin(); plit != cl->aend(); ++plit) {
        NDBG(cout << "    lit=" << *plit << ": ");
        BasicClauseList& l = o_list.clauses(-*plit);
        for (auto cl2 : l) {
          if (cl2->removed()) { continue; }
          // skip if tautology and doing resolution graph
          NDBG(cl2->dump(); cout << " (tl=" << num_tl_clause(model, cl2) << ") ";);
          if (rgraph && Utils::taut_resolvent(cl, cl2, *plit)) {
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
            GID gid2 = cl2->get_grp_id();
            if (gid2 && (gid2 != start_gid) && !md.nec(gid2)) {
              NDBG(cout << "  got non-g0 clause"; cl2->dump(); cout << " path: ");
              for (BasicClause* c = cl2; c->incoming_parent() != 0; 
                   c = c->incoming_parent()) {
                NDBG(cout << " from "; c->parent->dump(); cout << " on " << c->lit << ", ";);
                Utils::flip(model, abs(c->incoming_lit()));
              }
              NDBG(cout << endl;);
              NDBG(cout << "  found candidate: " << gid2 << endl;);
              result = gid2;
              goto _done;
            }
            if ((max_points > 0) && (v_count > max_points))
              goto _done;
          }
        }
        NDBG(cout << endl;);
      }
      NDBG(cout << endl;);
      q.pop();
    }
  _done:
    // put together the multiflip ...
    DBG(cout << "  visited nodes = " << v_count << endl;);
    return result;
  }

} // anonymous namespace
