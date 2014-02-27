/*----------------------------------------------------------------------------*\
 * File:        rgraph_utils.hh
 *
 * Description: Declaration and implementation of functions for resolution graph
 *              analysis.
 *
 * Author:      antonb
 * 
 *                                               Copyright (c) 2012, Anton Belov
 \*----------------------------------------------------------------------------*/

#ifndef _RGRAPH_UTILS_HH
#define _RGRAPH_UTILS_HH 1

#include <cassert>
#include <cstdlib>
#include <queue>
#include <sys/time.h>
#include <vector>
#include "globals.hh"
#include "basic_clause.hh"
#include "basic_group_set.hh"
#include "utils.hh"

//#define DBG(x) x

namespace RGraphUtils {


  /** Starting from the given set of falsified clauses, searches the conflict 
   * (or resolution) graph for a path to some clause with a GID among the 
   * specified set of target_gids. If found, the target clause is returned, and 
   * the path starting from the initial clause (in fclauses) is populated into 
   * in a vector of variables 'path' (if not 0). The function returns 0 and the 
   * path vector is empty if nothing is found. The search space size can be 
   * controlled by passing in the max_points parameter (if > 0) and new_search
   * parameters.
   */
  template<class C>
  BasicClause* find_target(const BasicGroupSet& gset,        // in
                           const C& fclauses,                // in 
                           const GIDSet& target_gids,        // in
                           bool new_search,                  // in
                           unsigned max_points,              // in
                           bool use_rgraph,                  // in
                           std::vector<ULINT>* path)         // out
  {
    assert((!path || path->empty()) && "out vector should be empty");
    const OccsList& o_list = gset.occs_list();
    BasicClause* result = 0;
    if (path) { path->clear(); }

    static unsigned visited_gen = 1;
    if (new_search) { visited_gen++; }
    
    std::queue<BasicClause*> q;      // queue for doing BFS
    unsigned v_count = 0;            // count of visited points

    // initialize the search
    for (BasicClause* cl : fclauses) {
      DBG(cout << "  initial clause: "; cl->dump(); cout << endl;);
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
      DBG(cout << "  clause: "; cl->dump(); cout << ", neighbours: " << endl;);
      for (auto plit = cl->abegin(); plit != cl->aend(); ++plit) {
        NDBG(cout << "    lit=" << *plit << ": ");
        const BasicClauseList& l = o_list.clauses(-*plit);
        if (l.size() > 100) { continue; }
        for (auto cl2 : l) {
          if (cl2->removed()) { continue; }
          // skip if tautology and doing resolution graph
          NDBG(cl2->dump(););
          if (use_rgraph && Utils::taut_resolvent(cl, cl2, *plit)) { // might be expensive !
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
              DBG(cout << endl << "  target gid: " << cl2->get_grp_id() << endl;);
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
    DBG(cout << "  visited nodes = " << v_count << endl;);
    return result;
  }

}

#endif


