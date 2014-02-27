/*----------------------------------------------------------------------------*\
 * File:        res_graph.hh
 *
 * Description: Class definition of the resolution graph.
 *
 * Author:      antonb
 * 
 * Notes:
 *      1. IMPORTANT: this implementation is NOT multi-thread safe.
 *
 *                                              Copyright (c) 2012, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _RES_GRAPH_HH
#define _RES_GRAPH_HH

#include <boost/config.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>
#include <ostream>
#include <unordered_map>
#include "basic_clause.hh"
#include "basic_group_set.hh"
#include "cl_types.hh"

/*----------------------------------------------------------------------------*\
 * Class:  ResGraph
 *
 * Purpose: A class that maintains the resolution graph of a group set, and
 *          knows to work with it.
 *
 * Notes:
 *
\*----------------------------------------------------------------------------*/

class ResGraph {

  // types (note listS for edges; may want to try setS and hash_setS)
  typedef boost::adjacency_list <
    boost::listS, boost::listS, boost::undirectedS, BasicClause*, ULINT> Graph; 
  typedef boost::graph_traits<Graph>::vertex_descriptor Vertex;
  typedef boost::graph_traits<Graph>::vertex_iterator VertexIter;
  typedef boost::graph_traits<Graph>::edge_descriptor Edge;
  typedef boost::graph_traits<Graph>::out_edge_iterator EdgeIter;
  typedef std::unordered_map<const BasicClause*, Vertex, ClPtrHash, ClPtrEqual> VMap;

public:       // Lifecycle

  /* True if the graph is empty */
  bool empty(void) const { return true; }

  /* Clears everything out */
  void clear(void) { _g.clear(); _vmap.clear(); _rn.clear(); }

  /* Constructs the graph from the given group-set; this assumes that occlist
   * is populated in gs
   */
  void construct(BasicGroupSet& gs);

public:     // Queries
  
  /* Returns true if the clause is in the graph */
  bool has_clause(const BasicClause* cl) const { return _vmap.find(cl) != _vmap.end(); }

  /* Returns degree of the clause in the graph, -1 if not there */
  int degree(const BasicClause* cl) const {
    auto pvm = _vmap.find(cl);
    return (pvm != _vmap.end()) ? out_degree(pvm->second, _g) : -1;
  }

  /* Populates the vector with 1-neighbourhood of the clause; returns false
   * if the clause is not there; does not clear the vector
   */
  bool get_1hood(const BasicClause* cl, BasicClauseVector& hood) const;

public:     // Modifications

  /* Removes a clause from the graph; returns true if the clause was there */
  bool remove_clause(const BasicClause* cl);

  /* Returns a reference to vector with the clause neighours of the most 
   * recently removed clause
   */
  const BasicClauseVector& removed_nhood(void) const { return _rn; }

public:     // Debugging

  void dump(std::ostream& out = std::cout) const {
    out << "Resolution graph: " << std::endl;
  }

  friend std::ostream& operator<<(std::ostream& out, const ResGraph& rg) {
    rg.dump(out);
    return out;
  }

private:

  Graph _g;                // the graph itself

  VMap _vmap;              // map from BasicClause* to Vertex

  BasicClauseVector _rn;   // neighbours of the most recently removed vertex

  // stats

  double _c_time;       // construction time
  
  double _cc_time;      // connected components time

};

#endif /* _RES_GRAPH_HH */

/*----------------------------------------------------------------------------*/
