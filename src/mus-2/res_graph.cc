/*----------------------------------------------------------------------------*\
 * File:        res_graph.cc
 *
 * Description: Implementation of the resolution graph.
 *
 * Author:      antonb
 * 
 * Notes:
 *      1. IMPORTANT: this implementation is NOT multi-thread safe.
 *
 *                                              Copyright (c) 2012, Anton Belov
\*----------------------------------------------------------------------------*/

#include <boost/config.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/betweenness_centrality.hpp>
#include <boost/graph/biconnected_components.hpp>
#include <boost/graph/connected_components.hpp>
#include <ostream>
#include <unordered_map>
#include "res_graph.hh"
#include "utils.hh"

using namespace std;
using namespace boost;

//#define DBG(x) x

/* Constructs the graph from the given group-set 
 */
void ResGraph::construct(BasicGroupSet& gs)
{
  assert(gs.has_occs_list()); // should have an occs list

  double s_time;

  // two options: one is to go through the pairs of occslists; another is to go 
  // through the clauses, and look into occlists for the candidates; the second one
  // seems to be always faster 
  s_time = -RUSAGE::read_cpu_time();
  OccsList& occs = gs.occs_list();
  for (ULINT var = 1; var <= gs.max_var(); ++var) {
    unsigned as_p = occs.active_size(var);
    unsigned as_n = occs.active_size(-var);
    if (!as_p || !as_n)
      continue;
    BasicClauseList& l1 = (as_p <= as_n) ? occs.clauses(var) : occs.clauses(-var);
    BasicClauseList& l2 = (as_p > as_n) ? occs.clauses(var) : occs.clauses(-var);
    for (BasicClause* cl : l1) {
      if (cl->removed())
        continue;
      // create a vertex, if needed
      auto pvm = _vmap.find(cl);
      if (pvm == _vmap.end()) {
        Vertex v = add_vertex(cl, _g);
        NDBG(cout << "New vertex: vd=" << v << ", cl=" << *_g[v] << endl;);
        pvm = _vmap.insert({ cl, v }).first;
      }
      Vertex& v_cl = pvm->second;
      for (BasicClause* o_cl : l2) {
        if (o_cl->removed() || Utils::taut_resolvent(cl, o_cl, var))
          continue;
        // create vertex if needed
        auto pvm = _vmap.find(o_cl);
        if (pvm == _vmap.end()) {
          Vertex v = add_vertex(o_cl, _g);
          NDBG(cout << "New vertex: vd=" << v << ", cl=" << *_g[v] << endl;);
          pvm = _vmap.insert({ o_cl, v }).first;
        }
        Vertex& v_o_cl = pvm->second;
        Edge e = add_edge(v_cl, v_o_cl, var, _g).first;
        NDBG(cout << "New edge: ed=" << e << ", src_cl=" 
             << *_g[source(e, _g)] << ", trg_cl=" << *_g[target(e, _g)] 
             << ", var=" << _g[e] << endl;);
      }
    }
  }
  s_time += RUSAGE::read_cpu_time();
  cout << "c Resolution graph size: " << num_vertices(_g) << " vertices, " 
       << num_edges(_g) << " edges"
       << ", construction time: " << s_time << " sec." << endl;

#if 0
  // try out a few things ...
  vector<int> component(num_vertices(_g));
  s_time = -RUSAGE::read_cpu_time();
  int num = connected_components(_g, &component[0]);
  s_time += RUSAGE::read_cpu_time();  
  cout << "c Total number of components: " << num 
       << ", construction time: " << s_time << " sec." << endl;
  vector<unsigned> comp_sizes(num, 0);
  for (Vertex vd = 0; vd != component.size(); ++vd)
    comp_sizes[component[vd]]++;
  cout << "c Component sizes:";
  for (unsigned sz : comp_sizes)
    cout << " " << sz;
  cout << endl;

  // articulation points
  vector<Vertex> art_points;
  s_time = -RUSAGE::read_cpu_time();
  articulation_points(_g, back_inserter(art_points));
  s_time += RUSAGE::read_cpu_time();  
  cout << "c Found " << art_points.size() << " articulation points"
       << ", construction time: " << s_time << " sec." << endl;

  // vertex centrality
  typedef property_map<Graph, vertex_index_t>::type VertexIndexMap;
  VertexIndexMap v_index = get(vertex_index, _g);
  vector<double> v_centrality_vec(num_vertices(_g), 0.0);
  // create the external property map
  iterator_property_map< vector<double>::iterator, VertexIndexMap>
    v_centrality_map(v_centrality_vec.begin(), v_index);
  s_time = -RUSAGE::read_cpu_time();
  brandes_betweenness_centrality(_g, v_centrality_map);
  s_time += RUSAGE::read_cpu_time();  
  cout << "c Betweenness centrality construction time: " << s_time << " sec." << endl;
#endif
}

/* Removes a clause from the graph; returns true if the clause was there; the
 * method also stores the neighbours into _rn vector.
 */
bool ResGraph::remove_clause(const BasicClause* cl) 
{
  auto pvm = _vmap.find(cl);
  if (pvm != _vmap.end()) {
    // remember the neighbours (and remove the edges)
    _rn.clear();
    EdgeIter ei, ei_end;       
    for (tie(ei, ei_end) = out_edges(pvm->second, _g); ei != ei_end; ) {
      EdgeIter old = ei++;
      _rn.push_back(_g[target(*old, _g)]);
      remove_edge(*old, _g);
    }
    //clear_vertex(pvm->second, _g);
    remove_vertex(pvm->second, _g);
    _vmap.erase(pvm);
    return true;
  }
  return false;
}

/* Populates the vector with 1-neighbourhood of the clause; returns false
 * if the clause is not there
 */
bool ResGraph::get_1hood(const BasicClause* cl, BasicClauseVector& hood) const
{
  auto pvm = _vmap.find(cl);
  if (pvm == _vmap.end())
    return false;
  EdgeIter ei, ei_end;       
  for (tie(ei, ei_end) = out_edges(pvm->second, _g); ei != ei_end; ++ei)
    hood.push_back(_g[target(*ei, _g)]);
  return true;
}

/*----------------------------------------------------------------------------*/


namespace {

} // anonymous namespace
