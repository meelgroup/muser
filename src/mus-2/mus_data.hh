/*----------------------------------------------------------------------------*\
 * File:        mus_data.hh
 *
 * Description: Class definition and implementation of MUSData container
 *
 * Author:      antonb
 * 
 * Notes:
 *
 * 1. MUSData is 1-to-1 with the initial group set
 * 2. Removed lists of clauses - everything is in terms of groups now.
 *    NOTE: if space becomes an issue, combine list and set into one set ordered
 *    by timestamp
 * 3. Not MT-safe
 * 4. Supports groups of variables now too.
 *
 *                                          Copyright (c) 2011-2012, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _MUS_DATA_HH
#define _MUS_DATA_HH 1

#include <algorithm>
#include <iostream>
#include <list>
#include "globals.hh"
#include "basic_group_set.hh"
#include "res_graph.hh"

/*----------------------------------------------------------------------------*\
 * Class: MUSData
 *
 * Purpose: Container for MUS extraction-related data
 *
 * Description:
 *
 * MUSData is a container for various information related to MUS extraction:
 * the group set, the removed groups, final groups, MUS groups etc. 
 *
 * The container has an interface for its own read-write lock -- in multithreaded 
 * environments it is the responsibility of callers to acquire the lock. The 
 * intended invariant is as follows: the moment a lock is acquired the nec_gids() 
 * contains the group IDs of groups that are necessary for gset() \setminus r_gids()
 *
 * The "version" number should be incremented whenever clauses or groups are
 * deemed removed. Version is important whenever unnesessary groups are detected
 * because groups that are unnessesary with respect to group set might not be
 * unnecessary with respect of its subset.
 *
\*----------------------------------------------------------------------------*/

class MUSData {

public:

  MUSData(BasicGroupSet& gset, bool var_mode = false) 
    : _gset(gset), _var_mode(var_mode), _version(0) {}

  virtual ~MUSData(void) { if (_has_rgraph) _rgraph.clear(); }

public:    // Accessors

  /* Returns a refence to the underlying group set */
  BasicGroupSet& gset(void) { return _gset; }
  const BasicGroupSet& gset(void) const { return _gset; }

  /* Returns true if groups are groups of variables, and not clauses */
  bool var_mode(void) const { return _var_mode; }

  /* Returns a reference to the set of group IDs of removed groups. Thus,
   * effectively, the instance represented by MUSData consists of
   * gset() \setminus r_gids()
   */
  GIDSet& r_gids(void) { return _r_gids; }
  const GIDSet& r_gids(void) const { return _r_gids; }

  /* Returns a reference to the set of groups that are known to be in all high-
   * level MUSes gset() \setminus r_gids()
   */
  GIDSet& nec_gids(void) { return _nec_gids; }
  const GIDSet& nec_gids(void) const { return _nec_gids; }

  /* Returns a reference to the list of removed groups, with the most recent in
   * the front
   */
  GIDList& r_list(void) { return _r_list; }
  const GIDList& r_list(void) const { return _r_list; }

  /* Returns a reference to the list of finalized groups, with the most recent
   * in front
   */
  GIDList& f_list(void) { return _f_list; }
  const GIDList& f_list(void) const { return _f_list; }

  /* Returns a reference to the set of groups whose status has been faked through
   * approximation; this is a subset of either nec_gids() or r_gids(), depending
   * on the approximation mechanism
   */
  GIDSet& fake_gids(void) { return _fake_gids; }
  const GIDSet& fake_gids(void) const { return _fake_gids; }

  /* Returns the actual size of the instance (number of non-removed groups)
   */
  unsigned real_gsize(void) const { return _gset.gsize() - _r_gids.size(); }

  /* Returns the number of untested groups (disregarding group 0)
   */
  unsigned num_untested(void) const { 
    return _gset.gsize() - _gset.has_g0() - (_nec_gids.size() + _r_gids.size());
  }

  /* Returns the number of fake groups
   */
  unsigned num_fake(void) const { return _fake_gids.size(); }

public:    // Updates

  /* Marks gid as removed: puts it into r_gids() and r_list(), and updates the
   * gset(). Also if has resolution graph, clears the vertices from the graph.
   * If fake = true, adds it to the set of fake gids.
   */
  void mark_removed(GID gid, bool fake = false) {
    assert(!r(gid) && !nec(gid));
    if (_has_rgraph && _rgraph_dynamic)
      for (BasicClause* cl : _gset.gclauses(gid))
        _rgraph.remove_clause(cl);
    _r_gids.insert(gid);
    _r_list.push_front(gid);
    _gset.remove_group(gid);
    if (fake) { _fake_gids.insert(gid); }
  }

  /* Marks gid as necessary: puts it into nec_gids() and f_list()
   * If fake = true, adds it to the set of fake gids.
   */
  void mark_necessary(GID gid, bool fake = false) {
    assert(!r(gid) && !nec(gid));
    _nec_gids.insert(gid);
    _f_list.push_front(gid);
    if (fake) { _fake_gids.insert(gid); }
  }

  /* Clears the lists */
  void clear_lists(void) { _f_list.clear(); _r_list.clear(); }

public:    // Status checks

  /* True if group with gid is removed */
  bool r(const GID& gid) const { return _r_gids.find(gid) != _r_gids.end(); }

  /* True if group with gid is necessary */
  bool nec(const GID& gid) const { return _nec_gids.find(gid) != _nec_gids.end(); }

  /* True if group with gid is untested (neither necessary nor removed */
  bool untested(const GID& gid) const { return !r(gid) && !nec(gid); }
 
public:    // Versioning

  /* Returns current version number */
  unsigned version(void) const { return _version; }

  /* Increments the current version number, and returns the new version */
  unsigned incr_version(void) { return ++_version; }

public:    // Lock functions (subclasses provide implementation)

  /* Get a read-lock on the object */
  virtual void lock_for_reading(void) const {}

  /* Get a write-lock on the object: only through non constant reference */
  virtual void lock_for_writing(void) {}

  /* Release lock */
  virtual void release_lock(void) const {}

public:    // Output functions

  /* Writes out the instance in competition format (i.e. "v " followed by 
   * group-ids); in case the tool was interrupted, the necessary GID's are
   * written out first, followed by unknown.
   */
  std::ostream& write_comp(std::ostream& out) {
    out << "c nec: " << _nec_gids.size() << " unk: " << num_untested() << std::endl;
    out << "v ";
    for (GID gid : _nec_gids) { out << gid << " "; }
    std::for_each(_gset.gbegin(), _gset.gend(), [&](GID gid) {
      if (gid && untested(gid)) { out << gid << " "; }});
    out << "0" << std::endl;
    return out;
  }

  /* Writes out the instance in cnf format -- this presumes that every group
   * contains exactly one clause, and that there is no group 0. Will throw 
   * logic_error if this is not the case. The option ignore_g0 can be used
   * to simply ignore group-0 clauses.
   * @param output_fmt - if 0, write out a CNF; if 1, also CNF, but put all
   * unknown clauses first; if 2, write a GCNF with all necessary clauses in
   * group 0.
   */
  std::ostream& write_cnf(std::ostream& out, bool ignore_g0 = false, int output_fmt = 0) {
    // check if finished, if not, add a comment
    if (real_gsize() != _nec_gids.size())
      out << "c WARNING: MUSer2 did not finish extraction; "
          << "this is an over-approximation of the result." << endl;
    out << "c " << num_untested() << " unknown clauses, " << _nec_gids.size()
        << " necessary clauses." << endl;
    if (output_fmt <= 1) {
      out << "p cnf " << _gset.max_var() << " "
          << (_gset.gsize() - _r_gids.size()) << std::endl;
    } else {
      assert(output_fmt == 2 && "invalid output_fmt value (should be 0,1,2)");
      out << "p gcnf " << _gset.max_var() << " "
          << (_gset.gsize() - _r_gids.size()) << " "
          << (num_untested()) << std::endl;
    }
    for (gset_iterator pgid = _gset.gbegin(); pgid != _gset.gend(); ++pgid) {
      if (*pgid == 0) {
        if (ignore_g0) { continue; }
        throw std::logic_error("MUSData::write_cnf(): found group 0,"
                               " this is not CNF");
      }
      if (!r(*pgid) && (!output_fmt || untested(*pgid))) {
        BasicClauseVector& gcl = pgid.gclauses();
        if (gcl.size() != 1)
          throw std::logic_error("MUSData::write_cnf(): found non-singleton "
                                 "group, this is not CNF");
        assert(!gcl[0]->removed());
        if (output_fmt == 2) { out << "{" << *pgid << "} "; }
        gcl[0]->awrite(out);
        out << std::endl;
      }
    }
    // for output_fmt > 0, still need to output necessary groups
    if (output_fmt) {
      for (GID gid : _nec_gids) {
        BasicClauseVector& gcl = _gset.gclauses(gid);
        if (gcl.size() != 1)
          throw std::logic_error("MUSData::write_cnf(): found non-singleton "
                                 "group, this is not CNF");
        assert(!gcl[0]->removed());
        if (output_fmt == 2) { out << "{0} "; }
        gcl[0]->awrite(out);
        out << std::endl;
      }
    }
    return out;
  }

  /* Writes out the instance in gcnf format.
   */
  std::ostream& write_gcnf(std::ostream& out) {
    // check if finished, if not, add a comment
    if (real_gsize()-1 != _nec_gids.size()) // TODO BUG: what if there's no G0 ?
      out << "c WARNING: MUSer2 did not finish extraction; "
          << "this is an over-approximation of the result." << endl;
    unsigned r_clauses = 0; // number of removed clauses
    for (gset_iterator pgid = _gset.gbegin(); pgid != _gset.gend(); ++pgid)
      if (r(*pgid))
        r_clauses += pgid.gclauses().size();
    out << "p gcnf " << _gset.max_var()
        <<  " " << _gset.size() - r_clauses
        <<  " " << _gset.max_gid() << std::endl;
    for (gset_iterator pgid = _gset.gbegin(); pgid != _gset.gend(); ++pgid) {
      if (!r(*pgid)) {
        BasicClauseVector& gcl = pgid.gclauses();
        for (ClVectIterator pcl = gcl.begin(); pcl != gcl.end(); ++pcl)
          if (!(*pcl)->removed()) {
            out << "{" << *pgid << "} ";
            (*pcl)->awrite(out);
            out << std::endl;
          }
      }
    }
    return out;
  }

  /* This method assumes that MUSData holds the information regarding a 
   * computed VMUS, and writes out the CNF instance of the formula induced
   * by the computed VMUS 
   */
  std::ostream& write_induced_cnf(std::ostream& out) {
    // check if finished, if not, add a comment
    if (real_gsize() != _nec_gids.size())
      out << "c WARNING: MUSer2 did not finish extraction; "
          << "this is an over-approximation of the result." << endl;
    // compute induced formula size
    unsigned if_size = 0;
    for (cvec_iterator pcl = _gset.begin(); pcl != _gset.end(); ++pcl)
      if_size += !((*pcl)->removed());
    out << "p cnf " << _nec_gids.size() << " " << if_size << std::endl;
    for (cvec_iterator pcl = _gset.begin(); pcl != _gset.end(); ++pcl)
      if (!(*pcl)->removed()) 
        out << **pcl << std::endl;
    return out;
  }

  /* This method assumes that MUSData holds the information regarding a 
   * computed VGMUS, and writes out the VGCNF instance of the formula induced
   * by the computed VGMUS 
   */
  std::ostream& write_induced_vgcnf(std::ostream& out) {
    // check if finished, if not, add a comment
    if (real_gsize()-1 != _nec_gids.size())
      out << "c WARNING: MUSer2 did not finish extraction; "
          << "this is an over-approximation of the result." << endl;
    // compute induced formula size
    unsigned if_size = 0;
    for (cvec_iterator pcl = _gset.begin(); pcl != _gset.end(); ++pcl)
      if_size += !((*pcl)->removed());
    out << "p vgcnf " << _gset.max_var() << " " << if_size << " " 
        << _gset.max_vgid() << std::endl;
    for (cvec_iterator pcl = _gset.begin(); pcl != _gset.end(); ++pcl)
      if (!(*pcl)->removed()) 
        out << **pcl << std::endl;
    // now all remaining groups
    for (vgset_iterator pvgid = _gset.vgbegin(); pvgid != _gset.vgend(); ++pvgid) {
      if (*pvgid && nec(*pvgid)) {
        out << "{" << *pvgid << "} ";
        VarVector& vars = pvgid.vgvars();
        copy(vars.begin(), vars.end(), ostream_iterator<int, char>(out, " "));
        out << "0" << endl;
      }
    }
    return out;
  }

public:         // Miscellaneous

  /* Makes all groups except group 0 unnecessary -- this is useful when g0 is 
   * deemed unsatisfiable (e.g. by BCP or other technique)
   */
  void make_empty_gmus(void) {
    _r_list.clear();
    for (gset_iterator pgid = _gset.gbegin(); pgid != _gset.gend(); ++pgid) {
      _r_gids.insert(*pgid);
      _r_list.push_back(*pgid);
    }
    _nec_gids.clear();
  }

public:         // Resolution graph

  /* True if the resolution graph has been built */
  bool has_rgraph(void) const { return _has_rgraph; }

  /* Build the resolution graph from the current content of associated gset; if
   * dynamic = true, the graph will be updated when clauses/groups are removed
   */
  void build_rgraph(bool dynamic = false) { 
    assert(!_has_rgraph); 
    _rgraph.construct(_gset); 
    _has_rgraph = true;
    _rgraph_dynamic = dynamic;
  }

  /* Destroys the resolution graph */
  void destroy_rgraph(void) {
    assert(_has_rgraph);
    _rgraph.clear();
    _has_rgraph = false;
  }

  /* Returns the reference to the resolution graph (assuming it built) -- don't clear
   * it yourself */
  ResGraph& rgraph(void) {  assert(_has_rgraph); return _rgraph; }
  const ResGraph& rgraph(void) const {  assert(_has_rgraph); return _rgraph; }

public:         // Experimental: potentially necessary

  /* Returns a reference to the set of groups that are potentially necessary (currently
   * this is a subset for nec_gids, its a hack)
   */
  GIDSet& pot_nec_gids(void) { return _pot_nec_gids; }
  const GIDSet& pot_nec_gids(void) const { return _pot_nec_gids; }

  bool pot_nec(GID gid) { return _pot_nec_gids.find(gid) != _pot_nec_gids.end(); }

protected:

  BasicGroupSet& _gset;        // the group set

  bool _var_mode;              // when true, groups are groups of variables, and not clauses

  GIDSet _r_gids;              // removed GIDs

  GIDSet _nec_gids;            // necessary GIDs of _gset \setminus _r_gids

  GIDList _r_list;             // ordered list of removed GIDs: most recent first

  GIDList _f_list;             // ordered list of finalized GIDs: most recent first

  unsigned _version;           // version number

  bool _has_rgraph = false;    // true when resolution graph has been constructed

  ResGraph _rgraph;            // resolution graph

  bool _rgraph_dynamic = false;// if true, update _rgraph when clauses are removed

  GIDSet _pot_nec_gids;        // TEMP: this is a subset of _nec_gids that points
                               // those groups that are no proven to be necessary
                               // but are "suspected" to be necessary

  GIDSet _fake_gids;           // group IDs whose status has been faked through
                               // approximation
};

#endif /* _MUS_DATA_H */

/*----------------------------------------------------------------------------*/
